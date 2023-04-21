/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#define HTTP_PORT 8000
#define HTTPS_PORT 4443

#if defined(CONFIG_NET_CONFIG_PEER_IPV6_ADDR)
#define SERVER_ADDR6  CONFIG_NET_CONFIG_PEER_IPV6_ADDR
#else
#define SERVER_ADDR6 ""
#endif

#if defined(CONFIG_NET_CONFIG_PEER_IPV4_ADDR)
#define SERVER_ADDR4  CONFIG_NET_CONFIG_PEER_IPV4_ADDR
#else
#define SERVER_ADDR4 ""
#endif

#define MAX_RECV_BUF_LEN 512
#define CONFIG_NET_SAMPLE_SEND_ITERATIONS 0

void http_client_example(void);

void http_get_example(void);

#endif /* _HTTP_CLIENT_H_ */
