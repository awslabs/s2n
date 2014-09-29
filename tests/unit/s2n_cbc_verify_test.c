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

#include "s2n_test.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <s2n.h>

#include "testlib/s2n_testlib.h"

#include "tls/s2n_cipher_suites.h"
#include "stuffer/s2n_stuffer.h"
#include "crypto/s2n_cipher.h"
#include "utils/s2n_random.h"
#include "utils/s2n_safety.h"
#include "crypto/s2n_hmac.h"
#include "tls/s2n_record.h"
#include "tls/s2n_prf.h"


/* qsort() u64s numerically */
static int u64cmp (const void * left, const void * right)
{
   return *(uint64_t *)left - *(uint64_t *)right;
}

/* Generate summary statistics from a list of u64s */
static void summarize(uint64_t *list, int n, uint64_t *count, uint64_t *avg, uint64_t *median, uint64_t *stddev, uint64_t *variance)
{
    qsort(list, n, sizeof(uint64_t), u64cmp);

    uint64_t p25 = list[ n / 4 ];
    uint64_t p50 = list[ n / 2 ];
    uint64_t p75 = list[ n - (n / 4)];
    uint64_t iqr = p75 - p25;

    *median = p50;
    *stddev = iqr;

    /* Use the standard interquartile range rule for outlier detection */
    int64_t floor = p25 - (iqr * 1.5);
    if (iqr > p25) {
        floor = 0;
    }

    *avg = floor;
        
    int64_t ceil = p75 + (iqr * 1.5);
    /* Ignore overflow as we have plenty of room at the top */

    *count = 0;
    uint64_t sum = 0;
    uint64_t sum_squares = 0;
    uint64_t min = 0xFFFFFFFF;
    uint64_t max = 0;
    
    for (int i = 0; i < n; i++) {
        int64_t value = list[ i ];

        if (value < floor || value > ceil) {
            continue;
        }

        (*count)++;

        sum += value;
        sum_squares += value * value;

        if (value < min) {
            min = value; 
        }
        if (value > max) {
            max = value;
        }
    }

    *avg = sum / *count;
    *variance = sum_squares - (sum * sum);
    *stddev = sqrt((*count * *variance) / (*count * (*count - 1)));
    *median = p50;
}

inline static uint64_t rdtsc(){
    unsigned int bot, top;
    __asm__ __volatile__ ("rdtsc" : "=a" (bot), "=d" (top));
    return ((uint64_t) top << 32) | bot;
}

int main(int argc, char **argv)
{
    struct s2n_connection *conn;
    uint8_t mac_key[] = "sample mac key";
    uint8_t fragment[S2N_MAXIMUM_FRAGMENT_LENGTH];
    uint8_t random_data[S2N_MAXIMUM_FRAGMENT_LENGTH];
    struct s2n_hmac_state check_mac, record_mac;

    BEGIN_TEST();

    EXPECT_SUCCESS(s2n_init(&err));
    EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_SERVER, &err));
    EXPECT_SUCCESS(s2n_get_random_data(random_data, S2N_MAXIMUM_FRAGMENT_LENGTH, &err));

    /* Emulate TLS1.2 */
    conn->actual_protocol_version = S2N_TLS12;

    /* Try every 16 bytes to simulate block alignments */
    for (int i = 320; i < S2N_MAXIMUM_FRAGMENT_LENGTH; i += 16) {

        EXPECT_SUCCESS(s2n_hmac_init(&record_mac, S2N_HMAC_SHA1, mac_key, sizeof(mac_key), &err));

        memcpy(fragment, random_data, i - 20 - 1);
        EXPECT_SUCCESS(s2n_hmac_update(&record_mac, fragment, i - 20 - 1, &err));
        EXPECT_SUCCESS(s2n_hmac_digest(&record_mac, fragment + (i - 20 - 1), 20, &err));

        /* Start out with zero byte padding */
        fragment[i - 1] = 0;
        struct s2n_blob decrypted = { .data = fragment, .size = i};

        uint64_t timings[10001];
        for (int t = 0; t < 10001; t++) {
            EXPECT_SUCCESS(s2n_hmac_init(&check_mac, S2N_HMAC_SHA1, mac_key, sizeof(mac_key), &err));

            uint64_t before = rdtsc();
            EXPECT_SUCCESS(s2n_verify_cbc(conn, &check_mac, &decrypted, &err)); 
            uint64_t after = rdtsc();

            timings[ t ] = (after - before);
        }

        uint64_t good_median, good_avg, good_stddev, good_variance, good_count;
        summarize(timings, 10001, &good_count, &good_avg, &good_median, &good_stddev, &good_variance);

        for (int t = 0; t < 10001; t++) {
            EXPECT_SUCCESS(s2n_hmac_init(&check_mac, S2N_HMAC_SHA1, mac_key, sizeof(mac_key), &err));

            uint64_t before = rdtsc();
            EXPECT_SUCCESS(s2n_verify_cbc(conn, &check_mac, &decrypted, &err)); 
            uint64_t after = rdtsc();

            timings[ t ] = (after - before);
        }

        summarize(timings, 10001, &good_count, &good_avg, &good_median, &good_stddev, &good_variance);

        /* Set up a record so that the MAC fails */
        EXPECT_SUCCESS(s2n_hmac_init(&record_mac, S2N_HMAC_SHA1, mac_key, sizeof(mac_key), &err));

        /* Set up 250 bytes of padding */
        for (int j = 1; j < 252; j++) {
            fragment[i - j] = 250;
        }

        memcpy(fragment, random_data, i - 20 - 251);
        EXPECT_SUCCESS(s2n_hmac_update(&record_mac, fragment, i - 20 - 251, &err));
        EXPECT_SUCCESS(s2n_hmac_digest(&record_mac, fragment + (i - 20 - 251), 20, &err));

        /* Verify that the record would pass: the MAC and padding are ok */
        EXPECT_SUCCESS(s2n_hmac_init(&check_mac, S2N_HMAC_SHA1, mac_key, sizeof(mac_key), &err));
        EXPECT_SUCCESS(s2n_verify_cbc(conn, &check_mac, &decrypted, &err)); 

        /* Corrupt a HMAC byte */
        fragment[i - 255]++;

        for (int t = 0; t < 10001; t++) {
            EXPECT_SUCCESS(s2n_hmac_init(&check_mac, S2N_HMAC_SHA1, mac_key, sizeof(mac_key), &err));

            uint64_t before = rdtsc();
            EXPECT_FAILURE(s2n_verify_cbc(conn, &check_mac, &decrypted, &err)); 
            uint64_t after = rdtsc();

            timings[ t ] = (after - before);
        }
        
        uint64_t mac_median, mac_avg, mac_stddev, mac_variance, mac_count;
        summarize(timings, 10001, &mac_count, &mac_avg, &mac_median, &mac_stddev, &mac_variance);

        /* Use a simple 3 sigma test for the median distance from the good */
        int64_t lo = good_median - (3 * good_stddev);
        int64_t hi = good_median + (3 * good_stddev);

        if ((int64_t) mac_median < lo || (int64_t) mac_median > hi) {
            printf("\n\nRecord size: %d\nGood Median: %llu (Avg: %llu Stddev: %llu)\n"
                   "Bad Median: %llu (Avg: %llu Stddev: %llu)\n\n", 
                    i, good_median, good_avg, good_stddev, mac_median, mac_avg, mac_stddev);
            FAIL();
        }

        /* Set up the record so that the HMAC passes, and the padding fails */
        EXPECT_SUCCESS(s2n_hmac_init(&record_mac, S2N_HMAC_SHA1, mac_key, sizeof(mac_key), &err));

        /* Set up 15 bytes of padding */
        for (int j = 1; j < 17; j++) {
            fragment[i - j] = 15;
        }

        memcpy(fragment, random_data, i - 20 - 16);
        EXPECT_SUCCESS(s2n_hmac_update(&record_mac, fragment, i - 20 - 16, &err));
        EXPECT_SUCCESS(s2n_hmac_digest(&record_mac, fragment + (i - 20 - 16), 20, &err));

        /* Verify that the record would pass: the MAC and padding are ok */
        EXPECT_SUCCESS(s2n_hmac_init(&check_mac, S2N_HMAC_SHA1, mac_key, sizeof(mac_key), &err));
        EXPECT_SUCCESS(s2n_verify_cbc(conn, &check_mac, &decrypted, &err)); 

        /* Now corrupt a padding byte */
        fragment[i - 10]++;

        for (int t = 0; t < 10001; t++) {
            EXPECT_SUCCESS(s2n_hmac_init(&check_mac, S2N_HMAC_SHA1, mac_key, sizeof(mac_key), &err));

            uint64_t before = rdtsc();
            EXPECT_FAILURE(s2n_verify_cbc(conn, &check_mac, &decrypted, &err)); 
            uint64_t after = rdtsc();

            timings[ t ] = (after - before);
        }
        
        uint64_t pad_median, pad_avg, pad_stddev, pad_variance, pad_count;
        summarize(timings, 10001, &pad_count, &pad_avg, &pad_median, &pad_stddev, &pad_variance);

        /* Use a simple 3 sigma test for the median from the good */
        lo = good_median - (good_stddev);
        hi = good_median + (good_stddev);

        if ((int64_t) pad_median < lo || (int64_t) pad_median > hi) {
            printf("\n\nRecord size: %d\nGood Median: %llu (Avg: %llu Stddev: %llu)\n"
                   "Bad Median: %llu (Avg: %llu Stddev: %llu)\n\n", 
                    i, good_median, good_avg, good_stddev, pad_median, pad_avg, pad_stddev);
            FAIL();
        }
 
        /* Use a more sensitive 0.5 sigma test for the MAC error from the padding error. This is the
         * the difference that attackers can exploit.
         */
        lo = mac_median - (mac_stddev / 2);
        hi = mac_median + (mac_stddev / 2);

        if ((int64_t) pad_median < lo || (int64_t) pad_median > hi) {
            printf("\n\nRecord size: %dMAC Median: %llu (Avg: %llu Stddev: %llu)\n"
                   "PAD Median: %llu (Avg: %llu Stddev: %llu)\n\n", 
                    i, mac_median, mac_avg, mac_stddev, pad_median, pad_avg, pad_stddev);
            FAIL();
        }
    }

    END_TEST();
}
