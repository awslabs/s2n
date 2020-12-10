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

#include <s2n.h>

#include "tls/extensions/s2n_extension_type_lists.h"
#include "tls/s2n_connection.h"

#include "tls/extensions/s2n_cookie.h"
#include "tls/extensions/s2n_client_supported_versions.h"
#include "tls/extensions/s2n_client_signature_algorithms.h"
#include "tls/extensions/s2n_client_max_frag_len.h"
#include "tls/extensions/s2n_client_session_ticket.h"
#include "tls/extensions/s2n_client_server_name.h"
#include "tls/extensions/s2n_client_alpn.h"
#include "tls/extensions/s2n_client_status_request.h"
#include "tls/extensions/s2n_client_key_share.h"
#include "tls/extensions/s2n_client_sct_list.h"
#include "tls/extensions/s2n_client_supported_groups.h"
#include "tls/extensions/s2n_client_pq_kem.h"
#include "tls/extensions/s2n_client_psk.h"
#include "tls/extensions/s2n_client_renegotiation_info.h"
#include "tls/extensions/s2n_ec_point_format.h"
#include "tls/extensions/s2n_quic_transport_params.h"
#include "tls/extensions/s2n_server_certificate_status.h"
#include "tls/extensions/s2n_server_renegotiation_info.h"
#include "tls/extensions/s2n_server_alpn.h"
#include "tls/extensions/s2n_server_status_request.h"
#include "tls/extensions/s2n_server_sct_list.h"
#include "tls/extensions/s2n_server_max_fragment_length.h"
#include "tls/extensions/s2n_server_session_ticket.h"
#include "tls/extensions/s2n_server_server_name.h"
#include "tls/extensions/s2n_server_signature_algorithms.h"
#include "tls/extensions/s2n_server_supported_versions.h"
#include "tls/extensions/s2n_server_key_share.h"

static const s2n_extension_type *const client_hello_extensions[] = {
        &s2n_client_supported_versions_extension,
        &s2n_client_key_share_extension,
        &s2n_client_signature_algorithms_extension,
        &s2n_client_server_name_extension,
        &s2n_client_alpn_extension,
        &s2n_client_status_request_extension,
        &s2n_client_sct_list_extension,
        &s2n_client_max_frag_len_extension,
        &s2n_client_session_ticket_extension,
        &s2n_client_supported_groups_extension,
        &s2n_client_ec_point_format_extension,
        &s2n_client_pq_kem_extension,
        &s2n_client_renegotiation_info_extension,
        &s2n_client_cookie_extension,
        &s2n_quic_transport_parameters_extension,
        &s2n_psk_key_exchange_modes_extension,
        &s2n_client_psk_extension /* MUST be last */
};

static const s2n_extension_type *const tls12_server_hello_extensions[] = {
        &s2n_server_supported_versions_extension,
        &s2n_server_server_name_extension,
        &s2n_server_ec_point_format_extension,
        &s2n_server_renegotiation_info_extension,
        &s2n_server_alpn_extension,
        &s2n_server_status_request_extension,
        &s2n_server_sct_list_extension,
        &s2n_server_max_fragment_length_extension,
        &s2n_server_session_ticket_extension,
};

static const s2n_extension_type *const tls13_server_hello_extensions[] = {
        &s2n_server_supported_versions_extension,
        &s2n_server_key_share_extension,
        &s2n_server_cookie_extension,
};

static const s2n_extension_type *const encrypted_extensions[] = {
        &s2n_server_server_name_extension,
        &s2n_server_max_fragment_length_extension,
        &s2n_server_alpn_extension,
        &s2n_quic_transport_parameters_extension,
};

static const s2n_extension_type *const cert_req_extensions[] = {
        &s2n_server_signature_algorithms_extension,
};

static const s2n_extension_type *const certificate_extensions[] = {
        &s2n_tls13_server_status_request_extension,
        &s2n_server_sct_list_extension,
};

#define S2N_EXTENSION_LIST(list) { .extension_types = (list), .count = s2n_array_len(list) }

static s2n_extension_type_list extension_lists[] = {
        [S2N_EXTENSION_LIST_CLIENT_HELLO] = S2N_EXTENSION_LIST(client_hello_extensions),
        [S2N_EXTENSION_LIST_SERVER_HELLO_DEFAULT] = S2N_EXTENSION_LIST(tls12_server_hello_extensions),
        [S2N_EXTENSION_LIST_SERVER_HELLO_TLS13] = S2N_EXTENSION_LIST(tls13_server_hello_extensions),
        [S2N_EXTENSION_LIST_ENCRYPTED_EXTENSIONS] = S2N_EXTENSION_LIST(encrypted_extensions),
        [S2N_EXTENSION_LIST_CERT_REQ] = S2N_EXTENSION_LIST(cert_req_extensions),
        [S2N_EXTENSION_LIST_CERTIFICATE] = S2N_EXTENSION_LIST(certificate_extensions),
        [S2N_EXTENSION_LIST_EMPTY] = { .extension_types = NULL, .count = 0 },
};

int s2n_extension_type_list_get(s2n_extension_list_id list_type, s2n_extension_type_list **extension_list)
{
    notnull_check(extension_list);
    lt_check(list_type, s2n_array_len(extension_lists));

    *extension_list = &extension_lists[list_type];
    return S2N_SUCCESS;
}
