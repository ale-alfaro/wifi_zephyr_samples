/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#define MY_PORT 8080

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#define STACK_SIZE 4096

#define SERVER_CERTIFICATE_TAG 1

static const unsigned char server_certificate[] = {
#include "mt-http-server-cert.der.inc"
};

/* This is the private key in pkcs#8 format. */
static const unsigned char private_key[] = {
#include "mt-http-server-key.der.inc"
};
#else
#define STACK_SIZE 4096
#endif
#if IS_ENABLED(CONFIG_NET_TC_THREAD_COOPERATIVE)
#define THREAD_PRIORITY K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1)
#else
#define THREAD_PRIORITY K_PRIO_PREEMPT(8)
#endif

#define CONFIG_NET_SAMPLE_NUM_HANDLERS 2

void http_server_example(void);

#ifndef USE_BIG_PAYLOAD
#define USE_BIG_PAYLOAD 0
#endif

#define CHECK(r) { if (r == -1) { printf("Error: " #r "\n"); exit(1); } }

void dumb_http_server_example(void);

#endif /* _HTTP_SERVER_H_ */
