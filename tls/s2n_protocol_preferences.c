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

#include "tls/s2n_connection.h"
#include "error/s2n_errno.h"
#include "utils/s2n_safety.h"

S2N_RESULT s2n_protocol_preferences_append(struct s2n_blob *application_protocols, const uint8_t *protocol, uint8_t protocol_len)
{
    ENSURE_MUT(application_protocols);
    ENSURE_REF(protocol);

    /**
     *= https://tools.ietf.org/rfc/rfc7301#section-3.1
     *# Empty strings
     *# MUST NOT be included and byte strings MUST NOT be truncated.
     */
    ENSURE(protocol_len != 0, S2N_ERR_INVALID_APPLICATION_PROTOCOL);

    uint32_t prev_len = application_protocols->size;
    uint32_t new_len = prev_len + /* len prefix */ 1 + protocol_len;
    ENSURE(new_len <= UINT16_MAX, S2N_ERR_INVALID_APPLICATION_PROTOCOL);

    GUARD_AS_RESULT(s2n_realloc(application_protocols, new_len));

    struct s2n_stuffer protocol_stuffer = {0};
    GUARD_AS_RESULT(s2n_stuffer_init(&protocol_stuffer, application_protocols));
    GUARD_AS_RESULT(s2n_stuffer_skip_write(&protocol_stuffer, prev_len));
    GUARD_AS_RESULT(s2n_stuffer_write_uint8(&protocol_stuffer, protocol_len));
    GUARD_AS_RESULT(s2n_stuffer_write_bytes(&protocol_stuffer, protocol, protocol_len));

    return S2N_RESULT_OK;
}

S2N_RESULT s2n_protocol_preferences_set(struct s2n_blob *application_protocols, const char *const *protocols, int protocol_count)
{
    ENSURE_MUT(application_protocols);

    /* NULL value indicates no preference so free the previous blob */
    if (protocols == NULL || protocol_count == 0) {
        GUARD_AS_RESULT(s2n_free(application_protocols));
        return S2N_RESULT_OK;
    }

    DEFER_CLEANUP(struct s2n_blob new_protocols = { 0 }, s2n_free);

    for (size_t i = 0; i < protocol_count; i++) {
        const uint8_t * protocol = (const uint8_t *)protocols[i];
        size_t length = strlen(protocols[i]);

        /**
         *= https://tools.ietf.org/rfc/rfc7301#section-3.1
         *# Empty strings
         *# MUST NOT be included and byte strings MUST NOT be truncated.
         */
        ENSURE(length < 256, S2N_ERR_INVALID_APPLICATION_PROTOCOL);

        GUARD_RESULT(s2n_protocol_preferences_append(&new_protocols, protocol, (uint8_t)length));
    }

    /* now we can free the previous list since we've validated all new input */
    GUARD_AS_RESULT(s2n_free(application_protocols));

    /* update the connection/config application_protocols with the newly allocated blob */
    *application_protocols = new_protocols;

    /* zero out new_protocols so we no longer refer to what we just allocated */
    new_protocols = (struct s2n_blob){ 0 };
    GUARD_AS_RESULT(s2n_free(&new_protocols));

    return S2N_RESULT_OK;
}

int s2n_config_set_protocol_preferences(struct s2n_config *config, const char *const *protocols, int protocol_count)
{
    return S2N_RESULT_TO_POSIX(s2n_protocol_preferences_set(&config->application_protocols, protocols, protocol_count));
}

int s2n_config_append_protocol_preference(struct s2n_config *config, const uint8_t *protocol, uint8_t protocol_len)
{
    return S2N_RESULT_TO_POSIX(s2n_protocol_preferences_append(&config->application_protocols, protocol, protocol_len));
}

int s2n_connection_set_protocol_preferences(struct s2n_connection *conn, const char * const *protocols, int protocol_count)
{
    return S2N_RESULT_TO_POSIX(s2n_protocol_preferences_set(&conn->application_protocols_overridden, protocols, protocol_count));
}

int s2n_connection_append_protocol_preference(struct s2n_connection *conn, const uint8_t *protocol, uint8_t protocol_len)
{
    return S2N_RESULT_TO_POSIX(s2n_protocol_preferences_append(&conn->application_protocols_overridden, protocol, protocol_len));
}
