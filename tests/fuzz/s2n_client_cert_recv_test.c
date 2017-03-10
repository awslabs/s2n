/*
 * Copyright 2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "api/s2n.h"
#include "stuffer/s2n_stuffer.h"
#include "tls/s2n_cipher_suites.h"
#include "tls/s2n_config.h"
#include "tls/s2n_connection.h"
#include "tls/s2n_crypto.h"
#include "tls/s2n_tls.h"
#include "tls/s2n_tls_parameters.h"
#include "utils/s2n_random.h"
#include "utils/s2n_safety.h"
#include "s2n_test.h"

static const uint8_t TLS_VERSIONS[] = {S2N_TLS10, S2N_TLS11, S2N_TLS12};

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    for(int version = 0; version < sizeof(TLS_VERSIONS); version++){
        /* Setup */
        struct s2n_connection *server_conn = s2n_connection_new(S2N_SERVER);
        notnull_check(server_conn);
        server_conn->actual_protocol_version = TLS_VERSIONS[version];
        server_conn->verify_cert_chain_callback = accept_all_rsa_certs;
        GUARD(s2n_stuffer_write_bytes(&server_conn->handshake.io, buf, len));

        /* Run Test
         * Do not use GUARD macro here since the connection memory hasn't been freed.
         */
        s2n_client_cert_recv(server_conn);

        /* Cleanup */
        GUARD(s2n_connection_free(server_conn));

    }
    return 0;
}
