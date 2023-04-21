/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define CA_CERTIFICATE_TAG 1

#define TLS_PEER_HOSTNAME "localhost"

/* This is the same cert as what is found in net-tools/https-cert.pem file
 */
static const unsigned char http_client_ca_certificate[] = {
#include "https-cert.der.inc"
};

/* By default only certificates in DER format are supported. If you want to use
 * certificate in PEM format, you can enable support for it in Kconfig.
 */

/* GlobalSign Root CA - R1 for https://google.com */
#if defined(CONFIG_TLS_CREDENTIAL_FILENAMES)
static const unsigned char http_get_ca_certificate[] = "globalsign_r1.der";
#else
static const unsigned char http_get_ca_certificate[] = {
#include "globalsign_r1.der.inc"
};
#endif