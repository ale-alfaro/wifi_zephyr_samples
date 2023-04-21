/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <net/wifi_mgmt_ext.h>
#include <net/wifi_credentials.h>

#include "message_channel.h"
#include "http_server.h"

/* Register log module */
LOG_MODULE_REGISTER(network, CONFIG_MQTT_SAMPLE_NETWORK_LOG_LEVEL);

K_SEM_DEFINE(wifi_connected_sem, 0, 1);

static void get_wifi_credential(void *cb_arg, const char *ssid, size_t ssid_len)
{
	struct wifi_credentials_personal config;

	wifi_credentials_get_by_ssid_personal_struct(ssid, ssid_len, &config);
	memcpy((struct wifi_credentials_personal *)cb_arg, &config, sizeof(config));
}

/* This module does not subscribe to any channels */
// BUILD_ASSERT(IS_ENABLED(CONFIG_WIFI_CREDENTIALS_STATIC), "Static Wi-Fi config must be used");

#define MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)

static struct net_mgmt_event_callback net_mgmt_callback;
static struct net_mgmt_event_callback net_mgmt_ipv4_callback;
static struct net_mgmt_event_callback net_mgmt_ipv4_multicast_callback;

static void connect(void)
{
	struct net_if *iface = net_if_get_default();

	if (iface == NULL) {
		LOG_ERR("Returned network interface is NULL");
		SEND_FATAL_ERROR();
		return;
	}

	int err = net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, iface, NULL, 0);

	if (err) {
		LOG_ERR("Connecting to Wi-Fi failed. error: %d", err);
		SEND_FATAL_ERROR();
	}
}


static struct in_addr mcast_addr = { { { 239, 255, 255, 250 } } };

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	int err;
	enum network_status status;
	const struct wifi_status *wifi_status = (const struct wifi_status *)cb->info;

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		if (wifi_status->status) {
			LOG_INF("Connection attempt failed, status code: %d", wifi_status->status);
			return;
		}

		LOG_INF("Wi-Fi Connected, waiting for IP address");

		return;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_INF("Disconnected");

		status = NETWORK_DISCONNECTED;
		break;
	default:
		LOG_ERR("Unknown event: %d", mgmt_event);
		return;
	}

	err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static void ipv4_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t event, struct net_if *iface)
{
	int err;
	enum network_status status;

	switch (event) {
	case NET_EVENT_IPV4_ADDR_ADD:
		LOG_INF("IPv4 address acquired");

		status = NETWORK_CONNECTED;
		k_sem_give(&wifi_connected_sem);
		break;
	case NET_EVENT_IPV4_ADDR_DEL:
		LOG_INF("IPv4 address lost");

		status = NETWORK_DISCONNECTED;
		break;
	default:
		LOG_DBG("Unknown event: 0x%08X", event);
		return;
	}

	err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static void ipv4_multicast_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t event, struct net_if *iface)
{
	int err;
	enum network_status status;

	switch (event) {
	case NET_EVENT_IPV4_MCAST_JOIN:
		LOG_INF("IPv4 multicast group joined");

		status = NETWORK_CONNECTED;
		break;
	case NET_EVENT_IPV4_MCAST_LEAVE:
		LOG_INF("IPv4 multicast group left");

		status = NETWORK_DISCONNECTED;
		break;
	default:
		LOG_DBG("Unknown event: 0x%08X", event);
		return;
	}

	err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

#ifdef UDP_MULTICAST_DISCOVERY
static int net_ipv4_multicast_join(struct net_if *iface, const struct in_addr *addr)
{
	struct net_if_mcast_addr *maddr;
	int ret;

	maddr = net_if_ipv4_maddr_lookup(addr, &iface);
	if (maddr && net_if_ipv4_maddr_is_joined(maddr)) {
		return -EALREADY;
	}

	if (!maddr) {
		maddr = net_if_ipv4_maddr_add(iface, addr);
		if (!maddr) {
			return -ENOMEM;
		}
	}

	net_if_ipv4_maddr_join(maddr);

	net_if_mcast_monitor(iface, &maddr->address, true);

	net_mgmt_event_notify_with_info(NET_EVENT_IPV4_MCAST_JOIN, iface,
					&maddr->address.in_addr,
					sizeof(struct in_addr));
	return ret;
}

static int net_ipv4_multicast_leave(struct net_if *iface, const struct in_addr *addr)
{
	struct net_if_mcast_addr *maddr;
	int ret;

	maddr = net_if_ipv4_maddr_lookup(addr, &iface);
	if (!maddr) {
		return -ENOENT;
	}

	if (!net_if_ipv4_maddr_rm(iface, addr)) {
		return -EINVAL;
	}

	net_if_ipv4_maddr_leave(maddr);

	net_if_mcast_monitor(iface, &maddr->address, false);

	net_mgmt_event_notify_with_info(NET_EVENT_IPV4_MCAST_LEAVE, iface,
					&maddr->address.in_addr,
					sizeof(struct in_addr));
	return ret;
}
#endif

static void network_task(void)
{
	net_mgmt_init_event_callback(&net_mgmt_callback, wifi_mgmt_event_handler, MGMT_EVENTS);
	net_mgmt_add_event_callback(&net_mgmt_callback);
	net_mgmt_init_event_callback(&net_mgmt_ipv4_callback, ipv4_mgmt_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_ADDR_DEL);
	net_mgmt_add_event_callback(&net_mgmt_ipv4_callback);
#ifdef UDP_MULTICAST_DISCOVERY
	net_mgmt_init_event_callback(&net_mgmt_ipv4_multicast_callback, ipv4_multicast_event_handler,
				     NET_EVENT_IPV4_MCAST_JOIN | NET_EVENT_IPV4_MCAST_LEAVE);
	net_mgmt_add_event_callback(&net_mgmt_ipv4_multicast_callback);
#endif

#if CONFIG_BT
	struct wifi_credentials_personal config = { 0 };
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params cnx_params = { 0 };

	k_sleep(K_SECONDS(5));
	/* Search for stored wifi credential and apply */
	wifi_credentials_for_each_ssid(get_wifi_credential, &config);
	if (config.header.ssid_len > 0) {
		LOG_INF("Configuration found. Try to apply.\n");

		cnx_params.ssid = config.header.ssid;
		cnx_params.ssid_length = config.header.ssid_len;
		cnx_params.security = config.header.type;

		cnx_params.psk = NULL;
		cnx_params.psk_length = 0;
		cnx_params.sae_password = NULL;
		cnx_params.sae_password_length = 0;

		if (config.header.type != WIFI_SECURITY_TYPE_NONE) {
			cnx_params.psk = config.password;
			cnx_params.psk_length = config.password_len;
		}

		cnx_params.channel = WIFI_CHANNEL_ANY;
		cnx_params.band = config.header.flags & WIFI_CREDENTIALS_FLAG_5GHz ?
				WIFI_FREQ_BAND_5_GHZ : WIFI_FREQ_BAND_2_4_GHZ;
		cnx_params.mfp = WIFI_MFP_OPTIONAL;
		int rc = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
			&cnx_params, sizeof(struct wifi_connect_req_params));
		if (rc < 0) {
			LOG_ERR("Cannot apply saved Wi-Fi configuration, err = %d.\n", rc);
		} else {
			LOG_INF("Configuration applied.\n");
		}
	}
#endif
	
#if CONFIG_WIFI_CREDENTIALS_STATIC
	/* Add temporary fix to prevent using Wi-Fi before WPA supplicant is ready. */
	k_sleep(K_SECONDS(1));
	connect();
#endif

#if !CONFIG_SMF
	/* Wait until Wi-Fi is provisioned before connecting. */
	k_sem_take(&wifi_connected_sem, K_FOREVER);
	k_sleep(K_SECONDS(3));
#endif

#if CONFIG_HTTP_CLIENT_EXAMPLE 
	LOG_INF("HTTP client example...");
	http_client_example();
#elif CONFIG_HTTP_GET_EXAMPLE 
	LOG_INF("HTTP client example...");
	http_get_example();
#elif CONFIG_DUMB_HTTP_SERVER_EXAMPLE
	LOG_INF("HTTP server example...");
	dumb_http_server_example();
#elif CONFIG_HTTP_SERVER_EXAMPLE
	LOG_INF("HTTP server example...");
	http_server_example();
#endif
	LOG_INF("Network thread init finished");
	
}

K_THREAD_DEFINE(network_task_id,
		CONFIG_MQTT_SAMPLE_NETWORK_THREAD_STACK_SIZE,
		network_task, NULL, NULL, NULL, 3, 0, 0);
