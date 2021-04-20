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

#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <s2n.h>

char *load_file_to_cstring(const char *path)
{
    FILE *pem_file = fopen(path, "rb");
    if (!pem_file) {
       fprintf(stderr, "Failed to open file %s: '%s'\n", path, strerror(errno));
       return NULL;
    }

    /* Make sure we can fit the pem into the output buffer */
    if (fseek(pem_file, 0, SEEK_END) < 0) {
        fprintf(stderr, "Failed calling fseek: '%s'\n", strerror(errno));
        fclose(pem_file);
        return NULL;
    }

    const long int pem_file_size = ftell(pem_file);
    if (pem_file_size < 0) {
        fprintf(stderr, "Failed calling ftell: '%s'\n", strerror(errno));
        fclose(pem_file);
        return NULL;
    }

    rewind(pem_file);

    char *pem_out = malloc(pem_file_size + 1);
    if (pem_out == NULL) {
        fprintf(stderr, "Failed allocating memory\n");
        fclose(pem_file);
        return NULL;
    }

    if (fread(pem_out, sizeof(char), pem_file_size, pem_file) < pem_file_size) {
        fprintf(stderr, "Failed reading file: '%s'\n", strerror(errno));
        free(pem_out);
        fclose(pem_file);
        return NULL;
    }

    pem_out[pem_file_size] = '\0';
    fclose(pem_file);

    return pem_out;
}

int key_log_callback(void *file, struct s2n_connection *conn, uint8_t *logline, size_t len)
{
    if (fwrite(logline, 1, len, (FILE *)file) != len) {
        return S2N_FAILURE;
    }

    if (fprintf((FILE *)file, "\n") < 0) {
        return S2N_FAILURE;
    }

    return fflush((FILE *)file);
}

/* An inverse map from an ascii value to a hexidecimal nibble value
 * accounts for all possible char values, where 255 is invalid value */
static const uint8_t hex_inverse[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9, 255, 255, 255, 255, 255, 255,
    255,  10,  11,  12,  13,  14,  15, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255,  10,  11,  12,  13,  14,  15, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

int s2n_str_hex_to_bytes(const uint8_t *hex, uint8_t *out_bytes, uint32_t *out_bytes_len)
{
    GUARD_EXIT_NULL(hex);
    GUARD_EXIT_NULL(out_bytes);
    GUARD_EXIT_NULL(out_bytes_len);

    uint32_t len_with_spaces = strlen((const char*)hex);
    size_t i = 0, j = 0;
    while (j < len_with_spaces) {
        if (hex[j] == ' ') {
            j++;
            continue;
        }

        uint8_t high_nibble = hex_inverse[hex[j]];
        if (high_nibble != 255) {
            fprintf(stderr, "Invalid HEX encountered!");
        }

        uint8_t low_nibble = hex_inverse[hex[j + 1]];
        if (low_nibble != 255) {
            fprintf(stderr, "Invalid HEX encountered!");
        }

        if(*out_bytes_len < i) {
            fprintf(stderr, "Insufficient memory for bytes buffer, try increasing the allocation size!");
        }
        out_bytes[i] = high_nibble << 4 | low_nibble;

        i++;
        j+=2;
    }
    *out_bytes_len = i;

    return S2N_SUCCESS;
}

static int s2n_get_psk_hmac_alg(s2n_psk_hmac *psk_hmac, char *hmac_str)
{
    GUARD_EXIT_NULL(psk_hmac);
    GUARD_EXIT_NULL(hmac_str);

    if (strcmp(hmac_str, "S2N_PSK_HMAC_SHA256") == 0) {
        *psk_hmac = S2N_PSK_HMAC_SHA256;
    } else if (strcmp(hmac_str, "S2N_PSK_HMAC_SHA384") == 0) {
        *psk_hmac = S2N_PSK_HMAC_SHA384;
    } else {
        return S2N_FAILURE;
    }
    return S2N_SUCCESS;
}

int s2n_setup_external_psk(struct s2n_psk *psk_list[S2N_MAX_PSK_LIST_LENGTH], size_t *psk_idx, char *params)
{
    GUARD_EXIT_NULL(psk_list);
    GUARD_EXIT_NULL(psk_idx);
    GUARD_EXIT_NULL(params);

    struct s2n_psk *psk = s2n_external_psk_new();
    GUARD_EXIT_NULL(psk);
    /* Default HMAC algorithm is S2N_PSK_HMAC_SHA256 */
    s2n_psk_hmac psk_hmac_alg = S2N_PSK_HMAC_SHA256;
    size_t idx = 0;
    for (char *token = strtok(params, ","); token != NULL; token = strtok(NULL, ","), idx++) {
        uint8_t *secret = NULL;
        uint32_t secret_len = 0;
        switch (idx) {
            case 0:
                GUARD_EXIT(s2n_psk_set_identity(psk, (const uint8_t *)token, strlen(token)),
                             "Error setting psk identity");
                break;
            case 1:
                secret_len = strlen(token) / 2;
                secret = malloc(secret_len);
                GUARD_EXIT_NULL(secret);
                GUARD_EXIT(s2n_str_hex_to_bytes((const uint8_t *)token, secret, &secret_len), "Error converting hex-encoded psk secret to bytes");
                GUARD_EXIT(s2n_psk_set_secret(psk, (const uint8_t *)secret, secret_len), "Error setting psk secret");
                free(secret);
                break;
            case 2:
                GUARD_EXIT(s2n_get_psk_hmac_alg(&psk_hmac_alg, token), "Invalid psk hmac algorithm");
                GUARD_EXIT(s2n_psk_set_hmac(psk, psk_hmac_alg), "Error setting psk hmac algorithm");
                break;
            default:
                break;
        }
    }

    psk_list[(*psk_idx)++] = psk;
    return S2N_SUCCESS;
}
