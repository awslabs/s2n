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

#include "s2n_test.h"

#include <string.h>

#include "testlib/s2n_testlib.h"

#include "stuffer/s2n_stuffer.h"

#include "crypto/s2n_hkdf.h"
#include "crypto/s2n_hmac.h"

#include "utils/s2n_blob.h"

#define NUM_TESTS 12
#define MAX_OUTPUT_SIZE 82
#define MAX_PSEUDO_RAND_KEY_SIZE 64

int s2n_hkdf_extract(
    struct s2n_hmac_state *hmac,
    s2n_hmac_algorithm alg,
    const struct s2n_blob *salt,
    const struct s2n_blob *key,
    struct s2n_blob *pseudo_rand_key);

struct hkdf_test_vector {
    s2n_hmac_algorithm alg;
    uint8_t in_key[80];
    uint32_t in_key_len;
    uint8_t salt[80];
    uint32_t salt_len;
    uint8_t info[80];
    uint32_t info_len;
    uint8_t pseudo_rand_key[MAX_PSEUDO_RAND_KEY_SIZE];
    uint32_t prk_len;
    uint8_t output[MAX_OUTPUT_SIZE];
    uint32_t output_len;
};

/* Test vectors #0-6 obtained from RFC 5869:
 * Includes SHA-256 and SHA-1 vectors
 *
 * Test vectors #7-11 obtained from Kullo Blog:
 * Includes SHA-512 vectors
 * https://www.kullo.net/blog/hkdf-sha-512-test-vectors/
 */
static struct hkdf_test_vector tests[] = {
    {
        S2N_HMAC_SHA256,
        { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
          0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b },
        22,
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c },
        13,
        { 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9 },
        10,
        { 0x07, 0x77, 0x09, 0x36, 0x2c, 0x2e, 0x32, 0xdf, 0x0d, 0xdc, 0x3f, 0x0d, 0xc4, 0x7b, 0xba, 0x63,
          0x90, 0xb6, 0xc7, 0x3b, 0xb5, 0x0f, 0x9c, 0x31, 0x22, 0xec, 0x84, 0x4a, 0xd7, 0xc2, 0xb3, 0xe5 },
        32,
        { 0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90, 0x43, 0x4f, 0x64, 0xd0, 0x36,
          0x2f, 0x2a, 0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c, 0x5d, 0xb0, 0x2d, 0x56,
          0xec, 0xc4, 0xc5, 0xbf, 0x34, 0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65 },
        42,
    },
    {
        S2N_HMAC_SHA256,
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
          0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
          0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
          0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
          0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f },
        80,
        { 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
          0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
          0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
          0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
          0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf },
        80,
        { 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
          0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
          0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
          0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
          0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff },
        80,
        { 0x06, 0xa6, 0xb8, 0x8c, 0x58, 0x53, 0x36, 0x1a, 0x06, 0x10, 0x4c, 0x9c, 0xeb, 0x35, 0xb4, 0x5c,
          0xef, 0x76, 0x00, 0x14, 0x90, 0x46, 0x71, 0x01, 0x4a, 0x19, 0x3f, 0x40, 0xc1, 0x5f, 0xc2, 0x44 },
        32,
        { 0xb1, 0x1e, 0x39, 0x8d, 0xc8, 0x03, 0x27, 0xa1, 0xc8, 0xe7, 0xf7, 0x8c, 0x59, 0x6a, 0x49, 0x34, 0x4f,
          0x01, 0x2e, 0xda, 0x2d, 0x4e, 0xfa, 0xd8, 0xa0, 0x50, 0xcc, 0x4c, 0x19, 0xaf, 0xa9, 0x7c, 0x59, 0x04,
          0x5a, 0x99, 0xca, 0xc7, 0x82, 0x72, 0x71, 0xcb, 0x41, 0xc6, 0x5e, 0x59, 0x0e, 0x09, 0xda, 0x32, 0x75,
          0x60, 0x0c, 0x2f, 0x09, 0xb8, 0x36, 0x77, 0x93, 0xa9, 0xac, 0xa3, 0xdb, 0x71, 0xcc, 0x30, 0xc5, 0x81,
          0x79, 0xec, 0x3e, 0x87, 0xc1, 0x4c, 0x01, 0xd5, 0xc1, 0xf3, 0x43, 0x4f, 0x1d, 0x87 },
        82,
    },
    {
        S2N_HMAC_SHA256,
        { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
          0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b },
        22,
        { 0x00 },
        0,
        { 0x00 },
        0,
        { 0x19, 0xef, 0x24, 0xa3, 0x2c, 0x71, 0x7b, 0x16, 0x7f, 0x33, 0xa9, 0x1d, 0x6f, 0x64, 0x8b, 0xdf,
          0x96, 0x59, 0x67, 0x76, 0xaf, 0xdb, 0x63, 0x77, 0xac, 0x43, 0x4c, 0x1c, 0x29, 0x3c, 0xcb, 0x04 },
        22,
        { 0x8d, 0xa4, 0xe7, 0x75, 0xa5, 0x63, 0xc1, 0x8f, 0x71, 0x5f, 0x80, 0x2a, 0x06, 0x3c,
          0x5a, 0x31, 0xb8, 0xa1, 0x1f, 0x5c, 0x5e, 0xe1, 0x87, 0x9e, 0xc3, 0x45, 0x4e, 0x5f,
          0x3c, 0x73, 0x8d, 0x2d, 0x9d, 0x20, 0x13, 0x95, 0xfa, 0xa4, 0xb6, 0x1a, 0x96, 0xc8 },
        42,
    },
    {
        S2N_HMAC_SHA1,
        { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b },
        11,
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c },
        13,
        { 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9 },
        10,
        { 0x9b, 0x6c, 0x18, 0xc4, 0x32, 0xa7, 0xbf, 0x8f, 0x0e, 0x71,
          0xc8, 0xeb, 0x88, 0xf4, 0xb3, 0x0b, 0xaa, 0x2b, 0xa2, 0x43 },
        20,
        { 0x08, 0x5a, 0x01, 0xea, 0x1b, 0x10, 0xf3, 0x69, 0x33, 0x06, 0x8b, 0x56, 0xef, 0xa5,
          0xad, 0x81, 0xa4, 0xf1, 0x4b, 0x82, 0x2f, 0x5b, 0x09, 0x15, 0x68, 0xa9, 0xcd, 0xd4,
          0xf1, 0x55, 0xfd, 0xa2, 0xc2, 0x2e, 0x42, 0x24, 0x78, 0xd3, 0x05, 0xf3, 0xf8, 0x96 },
        42,
    },
    {
        S2N_HMAC_SHA1,
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
          0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
          0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
          0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
          0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f },
        80,
        { 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
          0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
          0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
          0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
          0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf },
        80,
        { 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
          0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
          0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
          0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
          0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff },
        80,
        { 0x8a, 0xda, 0xe0, 0x9a, 0x2a, 0x30, 0x70, 0x59, 0x47, 0x8d,
          0x30, 0x9b, 0x26, 0xc4, 0x11, 0x5a, 0x22, 0x4c, 0xfa, 0xf6 },
        20,
        { 0x0b, 0xd7, 0x70, 0xa7, 0x4d, 0x11, 0x60, 0xf7, 0xc9, 0xf1, 0x2c, 0xd5, 0x91, 0x2a, 0x06, 0xeb, 0xff,
          0x6a, 0xdc, 0xae, 0x89, 0x9d, 0x92, 0x19, 0x1f, 0xe4, 0x30, 0x56, 0x73, 0xba, 0x2f, 0xfe, 0x8f, 0xa3,
          0xf1, 0xa4, 0xe5, 0xad, 0x79, 0xf3, 0xf3, 0x34, 0xb3, 0xb2, 0x02, 0xb2, 0x17, 0x3c, 0x48, 0x6e, 0xa3,
          0x7c, 0xe3, 0xd3, 0x97, 0xed, 0x03, 0x4c, 0x7f, 0x9d, 0xfe, 0xb1, 0x5c, 0x5e, 0x92, 0x73, 0x36, 0xd0,
          0x44, 0x1f, 0x4c, 0x43, 0x00, 0xe2, 0xcf, 0xf0, 0xd0, 0x90, 0x0b, 0x52, 0xd3, 0xb4 },
        82,
    },
    {
        S2N_HMAC_SHA1,
        { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
          0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b },
        22,
        {
            0x00,
        },
        0,
        {
            0x00,
        },
        0,
        { 0xda, 0x8c, 0x8a, 0x73, 0xc7, 0xfa, 0x77, 0x28, 0x8e, 0xc6,
          0xf5, 0xe7, 0xc2, 0x97, 0x78, 0x6a, 0xa0, 0xd3, 0x2d, 0x01 },
        20,
        { 0x0a, 0xc1, 0xaf, 0x70, 0x02, 0xb3, 0xd7, 0x61, 0xd1, 0xe5, 0x52, 0x98, 0xda, 0x9d,
          0x05, 0x06, 0xb9, 0xae, 0x52, 0x05, 0x72, 0x20, 0xa3, 0x06, 0xe0, 0x7b, 0x6b, 0x87,
          0xe8, 0xdf, 0x21, 0xd0, 0xea, 0x00, 0x03, 0x3d, 0xe0, 0x39, 0x84, 0xd3, 0x49, 0x18 },
        42,
    },
    {
        S2N_HMAC_SHA1,
        { 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
          0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c },
        22,
        { 0x00 },
        0,
        { 0x00 },
        0,
        { 0x2a, 0xdc, 0xca, 0xda, 0x18, 0x77, 0x9e, 0x7c, 0x20, 0x77,
          0xad, 0x2e, 0xb1, 0x9d, 0x3f, 0x3e, 0x73, 0x13, 0x85, 0xdd },
        20,
        { 0x2c, 0x91, 0x11, 0x72, 0x04, 0xd7, 0x45, 0xf3, 0x50, 0x0d, 0x63, 0x6a, 0x62, 0xf6,
          0x4f, 0x0a, 0xb3, 0xba, 0xe5, 0x48, 0xaa, 0x53, 0xd4, 0x23, 0xb0, 0xd1, 0xf2, 0x7e,
          0xbb, 0xa6, 0xf5, 0xe5, 0x67, 0x3a, 0x08, 0x1d, 0x70, 0xcc, 0xe7, 0xac, 0xfc, 0x48 },
        42,
    },
    {
        S2N_HMAC_SHA512,
        { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
          0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b },
        22,
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c },
        13,
        { 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9 },
        10,
        { 0x66, 0x57, 0x99, 0x82, 0x37, 0x37, 0xde, 0xd0, 0x4a, 0x88, 0xe4, 0x7e, 0x54, 0xa5, 0x89, 0x0b,
          0xb2, 0xc3, 0xd2, 0x47, 0xc7, 0xa4, 0x25, 0x4a, 0x8e, 0x61, 0x35, 0x07, 0x23, 0x59, 0x0a, 0x26,
          0xc3, 0x62, 0x38, 0x12, 0x7d, 0x86, 0x61, 0xb8, 0x8c, 0xf8, 0x0e, 0xf8, 0x02, 0xd5, 0x7e, 0x2f,
          0x7c, 0xeb, 0xcf, 0x1e, 0x00, 0xe0, 0x83, 0x84, 0x8b, 0xe1, 0x99, 0x29, 0xc6, 0x1b, 0x42, 0x37 },
        64,
        { 0x83, 0x23, 0x90, 0x08, 0x6c, 0xda, 0x71, 0xfb, 0x47, 0x62, 0x5b, 0xb5, 0xce, 0xb1,
          0x68, 0xe4, 0xc8, 0xe2, 0x6a, 0x1a, 0x16, 0xed, 0x34, 0xd9, 0xfc, 0x7f, 0xe9, 0x2c,
          0x14, 0x81, 0x57, 0x93, 0x38, 0xda, 0x36, 0x2c, 0xb8, 0xd9, 0xf9, 0x25, 0xd7, 0xcb },
        42,
    },
    {
        S2N_HMAC_SHA512,
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
          0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
          0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
          0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
          0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f },
        80,
        { 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
          0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
          0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
          0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
          0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf },
        80,
        { 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
          0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
          0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
          0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
          0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff },
        80,
        { 0x35, 0x67, 0x25, 0x42, 0x90, 0x7d, 0x4e, 0x14, 0x2c, 0x00, 0xe8, 0x44, 0x99, 0xe7, 0x4e, 0x1d,
          0xe0, 0x8b, 0xe8, 0x65, 0x35, 0xf9, 0x24, 0xe0, 0x22, 0x80, 0x4a, 0xd7, 0x75, 0xdd, 0xe2, 0x7e,
          0xc8, 0x6c, 0xd1, 0xe5, 0xb7, 0xd1, 0x78, 0xc7, 0x44, 0x89, 0xbd, 0xbe, 0xb3, 0x07, 0x12, 0xbe,
          0xb8, 0x2d, 0x4f, 0x97, 0x41, 0x6c, 0x5a, 0x94, 0xea, 0x81, 0xeb, 0xdf, 0x3e, 0x62, 0x9e, 0x4a },
        64,
        { 0xce, 0x6c, 0x97, 0x19, 0x28, 0x05, 0xb3, 0x46, 0xe6, 0x16, 0x1e, 0x82, 0x1e, 0xd1, 0x65, 0x67, 0x3b,
          0x84, 0xf4, 0x00, 0xa2, 0xb5, 0x14, 0xb2, 0xfe, 0x23, 0xd8, 0x4c, 0xd1, 0x89, 0xdd, 0xf1, 0xb6, 0x95,
          0xb4, 0x8c, 0xbd, 0x1c, 0x83, 0x88, 0x44, 0x11, 0x37, 0xb3, 0xce, 0x28, 0xf1, 0x6a, 0xa6, 0x4b, 0xa3,
          0x3b, 0xa4, 0x66, 0xb2, 0x4d, 0xf6, 0xcf, 0xcb, 0x02, 0x1e, 0xcf, 0xf2, 0x35, 0xf6, 0xa2, 0x05, 0x6c,
          0xe3, 0xaf, 0x1d, 0xe4, 0x4d, 0x57, 0x20, 0x97, 0xa8, 0x50, 0x5d, 0x9e, 0x7a, 0x93 },
        82,
    },
    {
        S2N_HMAC_SHA512,
        { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
          0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b },
        22,
        { 0x00 },
        0,
        { 0x00 },
        0,
        { 0xfd, 0x20, 0x0c, 0x49, 0x87, 0xac, 0x49, 0x13, 0x13, 0xbd, 0x4a, 0x2a, 0x13, 0x28, 0x71, 0x21,
          0x24, 0x72, 0x39, 0xe1, 0x1c, 0x9e, 0xf8, 0x28, 0x02, 0x04, 0x4b, 0x66, 0xef, 0x35, 0x7e, 0x5b,
          0x19, 0x44, 0x98, 0xd0, 0x68, 0x26, 0x11, 0x38, 0x23, 0x48, 0x57, 0x2a, 0x7b, 0x16, 0x11, 0xde,
          0x54, 0x76, 0x40, 0x94, 0x28, 0x63, 0x20, 0x57, 0x8a, 0x86, 0x3f, 0x36, 0x56, 0x2b, 0x0d, 0xf6 },
        64,
        { 0xf5, 0xfa, 0x02, 0xb1, 0x82, 0x98, 0xa7, 0x2a, 0x8c, 0x23, 0x89, 0x8a, 0x87, 0x03,
          0x47, 0x2c, 0x6e, 0xb1, 0x79, 0xdc, 0x20, 0x4c, 0x03, 0x42, 0x5c, 0x97, 0x0e, 0x3b,
          0x16, 0x4b, 0xf9, 0x0f, 0xff, 0x22, 0xd0, 0x48, 0x36, 0xd0, 0xe2, 0x34, 0x3b, 0xac },
        42,
    },
    {
        S2N_HMAC_SHA512,
        { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b },
        11,
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c },
        13,
        { 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9 },
        10,
        { 0x67, 0x40, 0x9c, 0x9c, 0xac, 0x28, 0xb5, 0x2e, 0xe9, 0xfa, 0xd9, 0x1c, 0x2f, 0xda, 0x99, 0x9f,
          0x7c, 0xa2, 0x2e, 0x34, 0x34, 0xf0, 0xae, 0x77, 0x28, 0x63, 0x83, 0x65, 0x68, 0xad, 0x6a, 0x7f,
          0x10, 0xcf, 0x11, 0x3b, 0xfd, 0xdd, 0x56, 0x01, 0x29, 0xa5, 0x94, 0xa8, 0xf5, 0x23, 0x85, 0xc2,
          0xd6, 0x61, 0xd7, 0x85, 0xd2, 0x9c, 0xe9, 0x3a, 0x11, 0x40, 0x0c, 0x92, 0x06, 0x83, 0x18, 0x1d },
        64,
        { 0x74, 0x13, 0xe8, 0x99, 0x7e, 0x02, 0x06, 0x10, 0xfb, 0xf6, 0x82, 0x3f, 0x2c, 0xe1,
          0x4b, 0xff, 0x01, 0x87, 0x5d, 0xb1, 0xca, 0x55, 0xf6, 0x8c, 0xfc, 0xf3, 0x95, 0x4d,
          0xc8, 0xaf, 0xf5, 0x35, 0x59, 0xbd, 0x5e, 0x30, 0x28, 0xb0, 0x80, 0xf7, 0xc0, 0x68 },
        42,
    },
    {
        S2N_HMAC_SHA512,
        { 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
          0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c },
        22,
        { 0x00 },
        0,
        { 0x00 },
        0,
        { 0x53, 0x46, 0xb3, 0x76, 0xbf, 0x3a, 0xa9, 0xf8, 0x4f, 0x8f, 0x6e, 0xd5, 0xb1, 0xc4, 0xf4, 0x89,
          0x17, 0x2e, 0x24, 0x4d, 0xac, 0x30, 0x3d, 0x12, 0xf6, 0x8e, 0xcc, 0x76, 0x6e, 0xa6, 0x00, 0xaa,
          0x88, 0x49, 0x5e, 0x7f, 0xb6, 0x05, 0x80, 0x31, 0x22, 0xfa, 0x13, 0x69, 0x24, 0xa8, 0x40, 0xb1,
          0xf0, 0x71, 0x9d, 0x2d, 0x5f, 0x68, 0xe2, 0x9b, 0x24, 0x22, 0x99, 0xd7, 0x58, 0xed, 0x68, 0x0c },
        64,
        { 0x14, 0x07, 0xd4, 0x60, 0x13, 0xd9, 0x8b, 0xc6, 0xde, 0xce, 0xfc, 0xfe, 0xe5, 0x5f,
          0x0f, 0x90, 0xb0, 0xc7, 0xf6, 0x3d, 0x68, 0xeb, 0x1a, 0x80, 0xea, 0xf0, 0x7e, 0x95,
          0x3c, 0xfc, 0x0a, 0x3a, 0x52, 0x40, 0xa1, 0x55, 0xd6, 0xe4, 0xda, 0xa9, 0x65, 0xbb },
        42,
    },
};

int main(int argc, char **argv) {
    struct s2n_hmac_state hmac;

    uint8_t prk_pad[MAX_PSEUDO_RAND_KEY_SIZE];
    struct s2n_blob prk_result = { .data = prk_pad, .size = sizeof(prk_pad) };

    uint8_t output_pad[MAX_OUTPUT_SIZE];
    struct s2n_blob out_result = { .data = output_pad, .size = sizeof(output_pad) };

    struct s2n_blob in_key_blob, salt_blob, info_blob, actual_prk_blob, actual_output_blob;

    BEGIN_TEST();

    EXPECT_SUCCESS(s2n_hmac_new(&hmac));

    for (uint8_t i = 0; i < NUM_TESTS; i++) {
        struct hkdf_test_vector *test = &tests[i];

        s2n_blob_init(&in_key_blob, test->in_key, test->in_key_len);
        s2n_blob_init(&salt_blob, test->salt, test->salt_len);
        s2n_blob_init(&info_blob, test->info, test->info_len);
        s2n_blob_init(&actual_prk_blob, test->pseudo_rand_key, test->prk_len);
        s2n_blob_init(&actual_output_blob, test->output, test->output_len);

        EXPECT_SUCCESS(s2n_hkdf_extract(&hmac, test->alg, &salt_blob, &in_key_blob, &prk_result));
        EXPECT_EQUAL(memcmp(prk_pad, actual_prk_blob.data, actual_prk_blob.size), 0);

        EXPECT_SUCCESS(s2n_hkdf(&hmac, test->alg, &salt_blob, &in_key_blob, &info_blob, &out_result));
        EXPECT_EQUAL(memcmp(output_pad, actual_output_blob.data, actual_output_blob.size), 0);
    }

    /* This size (5101) is obtained by multiplying the digest size for
     * SHA1 (20) by the maximum number of rounds allowed for HKDF (255),
     * then adding 1
     */
    uint8_t error_out_pad[5101];
    struct s2n_blob error_out = { .data = error_out_pad, .size = sizeof(error_out_pad) };
    struct s2n_blob zero_out  = { .data = output_pad, .size = 0 };

    s2n_hmac_algorithm alg = S2N_HMAC_SHA1;

    EXPECT_FAILURE(s2n_hkdf(&hmac, alg, &salt_blob, &in_key_blob, &info_blob, &error_out));
    EXPECT_FAILURE(s2n_hkdf(&hmac, alg, &salt_blob, &in_key_blob, &info_blob, &zero_out));

    EXPECT_SUCCESS(s2n_hmac_free(&hmac));

    END_TEST();
}
