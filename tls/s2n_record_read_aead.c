/*
 * Copyright 2014 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include <stdint.h>
#include <errno.h>
#include <time.h>

#include "error/s2n_errno.h"

#include "tls/s2n_cipher_suites.h"
#include "tls/s2n_connection.h"
#include "tls/s2n_record.h"
#include "tls/s2n_crypto.h"

#include "stuffer/s2n_stuffer.h"

#include "crypto/s2n_sequence.h"
#include "crypto/s2n_cipher.h"
#include "crypto/s2n_hmac.h"

#include "utils/s2n_safety.h"
#include "utils/s2n_random.h"
#include "utils/s2n_blob.h"

int s2n_record_parse_aead(const struct s2n_cipher_suite *cipher_suite,
			  struct s2n_connection *conn,
			  uint8_t content_type,
			  uint16_t encrypted_length,
			  uint8_t *implicit_iv,
			  struct s2n_hmac_state *mac,
			  uint8_t *sequence_number,
			  struct s2n_session_key *session_key)
{
    struct s2n_blob iv;
    struct s2n_blob en;
    struct s2n_blob aad;
    uint8_t aad_gen[S2N_TLS_MAX_AAD_LEN] = { 0 };
    uint8_t aad_iv[S2N_TLS_MAX_IV_LEN] = { 0 };
\
    en.size = encrypted_length;
    en.data = s2n_stuffer_raw_read(&conn->in, en.size);
    notnull_check(en.data);

    uint16_t payload_length = encrypted_length;

    /* In AEAD mode, the explicit IV is in the record */
    gte_check(en.size, cipher_suite->record_alg->cipher->io.aead.record_iv_size);

    struct s2n_stuffer iv_stuffer;
    iv.data = aad_iv;
    iv.size = sizeof(aad_iv);

    GUARD(s2n_stuffer_init(&iv_stuffer, &iv));

    if (cipher_suite->record_alg->flags & S2N_TLS12_AES_GCM_AEAD_NONCE) {
      /* Partially explicit nonce. See RFC 5288 Section 3 */
      GUARD(s2n_stuffer_write_bytes(&iv_stuffer, implicit_iv, cipher_suite->record_alg->cipher->io.aead.fixed_iv_size));
      GUARD(s2n_stuffer_write_bytes(&iv_stuffer, en.data, cipher_suite->record_alg->cipher->io.aead.record_iv_size));
    } else if (cipher_suite->record_alg->flags & S2N_TLS12_CHACHA_POLY_AEAD_NONCE) {
      /* Fully implicit nonce. See RFC 7905 Section 2 */
      uint8_t four_zeroes[4] = { 0 };
      GUARD(s2n_stuffer_write_bytes(&iv_stuffer, four_zeroes, 4));
      GUARD(s2n_stuffer_write_bytes(&iv_stuffer, sequence_number, S2N_TLS_SEQUENCE_NUM_LEN));
      for(int i = 0; i < cipher_suite->record_alg->cipher->io.aead.fixed_iv_size; i++) {
	aad_iv[i] = aad_iv[i] ^ implicit_iv[i];
      }
    } else {
      S2N_ERROR(S2N_ERR_INVALID_NONCE_TYPE);
    }

    /* Set the IV size to the amount of data written */
    iv.size = s2n_stuffer_data_available(&iv_stuffer);

    aad.data = aad_gen;
    aad.size = sizeof(aad_gen);

    /* remove the AEAD overhead from the record size */
    gte_check(payload_length, cipher_suite->record_alg->cipher->io.aead.record_iv_size + cipher_suite->record_alg->cipher->io.aead.tag_size);
    payload_length -= cipher_suite->record_alg->cipher->io.aead.record_iv_size;
    payload_length -= cipher_suite->record_alg->cipher->io.aead.tag_size;

    struct s2n_stuffer ad_stuffer;
    GUARD(s2n_stuffer_init(&ad_stuffer, &aad));
    GUARD(s2n_aead_aad_init(conn, sequence_number, content_type, payload_length, &ad_stuffer));
    
    /* Decrypt stuff! */
    /* Skip explicit IV for decryption */
    en.size -= cipher_suite->record_alg->cipher->io.aead.record_iv_size;
    en.data += cipher_suite->record_alg->cipher->io.aead.record_iv_size;

    /* Check that we have some data to decrypt */
    ne_check(en.size, 0);

    GUARD(cipher_suite->record_alg->cipher->io.aead.decrypt(session_key, &iv, &aad, &en, &en));
    struct s2n_blob seq = {.data = sequence_number,.size = S2N_TLS_SEQUENCE_NUM_LEN };
    GUARD(s2n_increment_sequence_number(&seq));

    /* O.k., we've successfully read and decrypted the record, now we need to align the stuffer
     * for reading the plaintext data.
     */
    GUARD(s2n_stuffer_reread(&conn->in));
    GUARD(s2n_stuffer_reread(&conn->header_in));

    /* Skip the IV, if any */
    if (conn->actual_protocol_version >= S2N_TLS12) {
      GUARD(s2n_stuffer_skip_read(&conn->in, cipher_suite->record_alg->cipher->io.aead.record_iv_size));
    }

    /* Truncate and wipe the MAC and any padding */
    GUARD(s2n_stuffer_wipe_n(&conn->in, s2n_stuffer_data_available(&conn->in) - payload_length));
    conn->in_status = PLAINTEXT;

    return 0;
}
