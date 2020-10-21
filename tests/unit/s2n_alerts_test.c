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
#include "tls/s2n_alerts.h"

#include "tls/s2n_quic_support.h"

#define ALERT_LEN (sizeof(uint16_t))

int main(int argc, char **argv)
{
    BEGIN_TEST();

    /* Test s2n_process_alert_fragment */
    {
        /* Safety check */
        EXPECT_FAILURE_WITH_ERRNO(s2n_process_alert_fragment(NULL), S2N_ERR_NULL);

        /* Fails if alerts not supported */
        {
            EXPECT_SUCCESS(s2n_enable_tls13());

            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));

            /* Succeeds by default */
            EXPECT_SUCCESS(s2n_stuffer_write_uint16(&conn->in, 0));
            EXPECT_SUCCESS(s2n_process_alert_fragment(conn));

            /* Wipe error */
            EXPECT_SUCCESS(s2n_stuffer_wipe(&conn->alert_in));

            /* Fails when alerts not supported (when QUIC mode enabled) */
            EXPECT_SUCCESS(s2n_connection_enable_quic(conn));
            EXPECT_SUCCESS(s2n_stuffer_write_uint16(&conn->in, 0));
            EXPECT_FAILURE_WITH_ERRNO(s2n_process_alert_fragment(conn), S2N_ERR_BAD_MESSAGE);

            EXPECT_SUCCESS(s2n_connection_free(conn));
            EXPECT_SUCCESS(s2n_disable_tls13());
        }

        /* Test warning behavior */
        {
            const uint8_t warning_alert[] = {  1 /* AlertLevel = warning */,
                                              70 /* AlertDescription = protocol_version (arbitrary value) */};

            const uint8_t user_canceled_alert[] = {  1 /* AlertLevel = warning */,
                                                    90 /* AlertDescription = user_canceled */ };

            /* Warnings treated as errors by default */
            {
                struct s2n_connection *conn;
                EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
                EXPECT_EQUAL(conn->config->alert_behavior, S2N_ALERT_FAIL_ON_WARNINGS);
                EXPECT_EQUAL(s2n_connection_get_protocol_version(conn), S2N_TLS12);

                EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, warning_alert, sizeof(warning_alert)));

                EXPECT_FAILURE_WITH_ERRNO(s2n_process_alert_fragment(conn), S2N_ERR_ALERT);
                EXPECT_TRUE(conn->closed);

                EXPECT_SUCCESS(s2n_connection_free(conn));
            }

            /* Warnings ignored in TLS1.2 if alert_behavior == S2N_ALERT_IGNORE_WARNINGS */
            {
                struct s2n_config *config;
                EXPECT_NOT_NULL(config = s2n_config_new());
                EXPECT_SUCCESS(s2n_config_set_alert_behavior(config, S2N_ALERT_IGNORE_WARNINGS));

                struct s2n_connection *conn;
                EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
                EXPECT_SUCCESS(s2n_connection_set_config(conn, config));
                EXPECT_EQUAL(s2n_connection_get_protocol_version(conn), S2N_TLS12);

                EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, warning_alert, sizeof(warning_alert)));

                EXPECT_SUCCESS(s2n_process_alert_fragment(conn));
                EXPECT_FALSE(conn->closed);

                EXPECT_SUCCESS(s2n_connection_free(conn));
                EXPECT_SUCCESS(s2n_config_free(config));
            }

            /* Warnings treated as errors in TLS1.3 if alert_behavior == S2N_ALERT_IGNORE_WARNINGS */
            {
                EXPECT_SUCCESS(s2n_enable_tls13());

                struct s2n_config *config;
                EXPECT_NOT_NULL(config = s2n_config_new());
                EXPECT_SUCCESS(s2n_config_set_alert_behavior(config, S2N_ALERT_IGNORE_WARNINGS));

                struct s2n_connection *conn;
                EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
                EXPECT_SUCCESS(s2n_connection_set_config(conn, config));
                EXPECT_EQUAL(s2n_connection_get_protocol_version(conn), S2N_TLS13);

                EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, warning_alert, sizeof(warning_alert)));

                EXPECT_FAILURE_WITH_ERRNO(s2n_process_alert_fragment(conn), S2N_ERR_ALERT);
                EXPECT_TRUE(conn->closed);

                EXPECT_SUCCESS(s2n_connection_free(conn));
                EXPECT_SUCCESS(s2n_config_free(config));
                EXPECT_SUCCESS(s2n_disable_tls13());
            }

            /* user_canceled ignored in TLS1.3 by default */
            {
                EXPECT_SUCCESS(s2n_enable_tls13());

                struct s2n_config *config;
                EXPECT_NOT_NULL(config = s2n_config_new());

                struct s2n_connection *conn;
                EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
                EXPECT_SUCCESS(s2n_connection_set_config(conn, config));
                EXPECT_EQUAL(s2n_connection_get_protocol_version(conn), S2N_TLS13);

                EXPECT_SUCCESS(s2n_stuffer_write_bytes(&conn->in, user_canceled_alert, sizeof(user_canceled_alert)));

                EXPECT_SUCCESS(s2n_process_alert_fragment(conn));
                EXPECT_FALSE(conn->closed);

                EXPECT_SUCCESS(s2n_connection_free(conn));
                EXPECT_SUCCESS(s2n_config_free(config));
                EXPECT_SUCCESS(s2n_disable_tls13());
            }
        }
    }

    /* Test s2n_queue_writer_close_alert_warning */
    {
        /* Safety check */
        EXPECT_FAILURE_WITH_ERRNO(s2n_queue_writer_close_alert_warning(NULL), S2N_ERR_NULL);

        /* Does not send alert if alerts not supported */
        {
            EXPECT_SUCCESS(s2n_enable_tls13());

            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->writer_alert_out), 0);

            /* Writes alert by default */
            EXPECT_SUCCESS(s2n_queue_writer_close_alert_warning(conn));
            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->writer_alert_out), ALERT_LEN);

            /* Wipe error */
            EXPECT_SUCCESS(s2n_stuffer_wipe(&conn->writer_alert_out));

            /* Does not write alert when alerts not supported (when QUIC mode enabled) */
            EXPECT_SUCCESS(s2n_connection_enable_quic(conn));
            EXPECT_SUCCESS(s2n_queue_writer_close_alert_warning(conn));
            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->writer_alert_out), 0);

            EXPECT_SUCCESS(s2n_connection_free(conn));
            EXPECT_SUCCESS(s2n_disable_tls13());
        }
    }

    /* Test s2n_queue_reader_alert
     *      Since s2n_queue_reader_alert is static, we'll test it indirectly via s2n_queue_reader_handshake_failure_alert */
    {
        /* Safety check */
        EXPECT_FAILURE_WITH_ERRNO(s2n_queue_reader_handshake_failure_alert(NULL), S2N_ERR_NULL);

        /* Does not send alert if alerts not supported */
        {
            EXPECT_SUCCESS(s2n_enable_tls13());

            struct s2n_connection *conn;
            EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_CLIENT));
            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->reader_alert_out), 0);

            /* Writes alert by default */
            EXPECT_SUCCESS(s2n_queue_reader_handshake_failure_alert(conn));
            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->reader_alert_out), ALERT_LEN);

            /* Wipe error */
            EXPECT_SUCCESS(s2n_stuffer_wipe(&conn->reader_alert_out));

            /* Does not write alert when alerts not supported (when QUIC mode enabled) */
            EXPECT_SUCCESS(s2n_connection_enable_quic(conn));
            EXPECT_SUCCESS(s2n_queue_reader_handshake_failure_alert(conn));
            EXPECT_EQUAL(s2n_stuffer_data_available(&conn->reader_alert_out), 0);

            EXPECT_SUCCESS(s2n_connection_free(conn));
            EXPECT_SUCCESS(s2n_disable_tls13());
        }
    }

    END_TEST();
}
