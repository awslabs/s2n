/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include "s2n_test.h"
#include "tls/s2n_quic_support.h"

#include "tls/s2n_connection.h"

static const uint8_t TEST_DATA[] = "test";

static int s2n_test_noop_secret_handler(void* context, struct s2n_connection *conn,
        s2n_secret_type_t secret_type, uint8_t *secret, uint8_t secret_size)
{
    return S2N_SUCCESS;
}

int main(int argc, char **argv)
{
    BEGIN_TEST();

    /* Test s2n_config_enable_quic */
    {
        struct s2n_config *config = s2n_config_new();
        EXPECT_NOT_NULL(config);
        EXPECT_FALSE(config->quic_enabled);

        /* Check error handling */
        {
            EXPECT_FAILURE_WITH_ERRNO(s2n_config_enable_quic(NULL), S2N_ERR_NULL);
            EXPECT_FALSE(config->quic_enabled);
        }

        /* Check success */
        {
            EXPECT_SUCCESS(s2n_config_enable_quic(config));
            EXPECT_TRUE(config->quic_enabled);

            /* Enabling QUIC again still succeeds */
            EXPECT_SUCCESS(s2n_config_enable_quic(config));
            EXPECT_TRUE(config->quic_enabled);
        }

        EXPECT_SUCCESS(s2n_config_free(config));
    }

    /* Test s2n_connection_set_quic_transport_parameters */
    {
        /* Safety checks */
        {
            struct s2n_connection conn = { 0 };
            EXPECT_FAILURE_WITH_ERRNO(s2n_connection_set_quic_transport_parameters(NULL, TEST_DATA, sizeof(TEST_DATA)),
                    S2N_ERR_NULL);
            EXPECT_FAILURE_WITH_ERRNO(s2n_connection_set_quic_transport_parameters(&conn, NULL, sizeof(TEST_DATA)),
                    S2N_ERR_NULL);
            EXPECT_SUCCESS(s2n_connection_set_quic_transport_parameters(&conn, TEST_DATA, 0));
            EXPECT_EQUAL(conn.our_quic_transport_parameters.size, 0);
        }

        /* Set transport data */
        {
            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));

            s2n_connection_set_quic_transport_parameters(conn, TEST_DATA, sizeof(TEST_DATA));
            EXPECT_BYTEARRAY_EQUAL(conn->our_quic_transport_parameters.data, TEST_DATA, sizeof(TEST_DATA));

            /* Set again */
            const uint8_t other_data[] = "other parameters";
            s2n_connection_set_quic_transport_parameters(conn, other_data, sizeof(other_data));
            EXPECT_BYTEARRAY_EQUAL(conn->our_quic_transport_parameters.data, other_data, sizeof(other_data));

            EXPECT_SUCCESS(s2n_connection_free(conn));
        }
    }

    /* Test s2n_connection_get_quic_transport_parameters */
    {
        /* Safety checks */
        {
            struct s2n_connection conn = { 0 };
            const uint8_t *data_buffer = NULL;
            uint16_t data_buffer_len = 0;

            EXPECT_FAILURE_WITH_ERRNO(s2n_connection_get_quic_transport_parameters(NULL, &data_buffer, &data_buffer_len),
                    S2N_ERR_NULL);
            EXPECT_FAILURE_WITH_ERRNO(s2n_connection_get_quic_transport_parameters(&conn, NULL, &data_buffer_len),
                    S2N_ERR_NULL);
            EXPECT_FAILURE_WITH_ERRNO(s2n_connection_get_quic_transport_parameters(&conn, &data_buffer, NULL),
                    S2N_ERR_NULL);
        }

        /* Get empty transport parameters */
        {
            const uint8_t *data_buffer = TEST_DATA;
            uint16_t data_buffer_len = sizeof(TEST_DATA);

            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));

            EXPECT_SUCCESS(s2n_connection_get_quic_transport_parameters(conn, &data_buffer, &data_buffer_len));
            EXPECT_EQUAL(data_buffer, NULL);
            EXPECT_EQUAL(data_buffer_len, 0);

            EXPECT_SUCCESS(s2n_connection_free(conn));
        }

        /* Get transport parameters */
        {
            const uint8_t *data_buffer = NULL;
            uint16_t data_buffer_len = 0;

            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));

            EXPECT_SUCCESS(s2n_alloc(&conn->peer_quic_transport_parameters, sizeof(TEST_DATA)));
            EXPECT_MEMCPY_SUCCESS(conn->peer_quic_transport_parameters.data, TEST_DATA, sizeof(TEST_DATA));

            EXPECT_SUCCESS(s2n_connection_get_quic_transport_parameters(conn, &data_buffer, &data_buffer_len));
            EXPECT_EQUAL(data_buffer, conn->peer_quic_transport_parameters.data);
            EXPECT_EQUAL(data_buffer_len, conn->peer_quic_transport_parameters.size);

            EXPECT_SUCCESS(s2n_connection_free(conn));
        }
    }

    /* Test s2n_connection_set_secret_callback */
    {
        uint8_t test_context;

        /* Safety checks */
        {
            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));

            EXPECT_FAILURE_WITH_ERRNO(s2n_connection_set_secret_callback(NULL, s2n_test_noop_secret_handler, &test_context), S2N_ERR_NULL);
            EXPECT_FAILURE_WITH_ERRNO(s2n_connection_set_secret_callback(conn, NULL, &test_context), S2N_ERR_NULL);

            EXPECT_EQUAL(conn->secret_cb, NULL);
            EXPECT_EQUAL(conn->secret_cb_context, NULL);

            EXPECT_SUCCESS(s2n_connection_free(conn));
        }

        /* Succeeds with NULL context */
        {
            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
            EXPECT_EQUAL(conn->secret_cb, NULL);
            EXPECT_EQUAL(conn->secret_cb_context, NULL);

            EXPECT_SUCCESS(s2n_connection_set_secret_callback(conn, s2n_test_noop_secret_handler, NULL));

            EXPECT_EQUAL(conn->secret_cb, s2n_test_noop_secret_handler);
            EXPECT_NULL(conn->secret_cb_context);

            EXPECT_SUCCESS(s2n_connection_free(conn));
        }

        /* Succeeds with context */
        {
            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
            EXPECT_EQUAL(conn->secret_cb, NULL);
            EXPECT_EQUAL(conn->secret_cb_context, NULL);

            EXPECT_SUCCESS(s2n_connection_set_secret_callback(conn, s2n_test_noop_secret_handler, &test_context));

            EXPECT_EQUAL(conn->secret_cb, s2n_test_noop_secret_handler);
            EXPECT_EQUAL(conn->secret_cb_context, &test_context);

            EXPECT_SUCCESS(s2n_connection_free(conn));
        }
    }

    END_TEST();
}
