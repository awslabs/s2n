/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
#include "testlib/s2n_testlib.h"
#include "stuffer/s2n_stuffer.h"
#include "tls/s2n_record.h"

#include <stdint.h>
#include <stdlib.h>

#include <s2n.h>

int main(int argc, char **argv)
{
    uint16_t plaintext = 0xdaf3;
    struct s2n_stuffer plaintext_stuffer = {0};
    uint8_t record_type;
    BEGIN_TEST();

   /* In tls13 the true record type is inserted in the last byte of the encrypted payload. This
    * test creates a fake unencrypted payload and checks that the helper function
    * s2n_parse_record_type() correctly parses the type.
    */
    {
        EXPECT_SUCCESS(s2n_stuffer_alloc(&plaintext_stuffer, sizeof(plaintext)));
        EXPECT_SUCCESS(s2n_stuffer_write_uint16(&plaintext_stuffer, plaintext));
        EXPECT_SUCCESS(s2n_parse_record_type(&plaintext_stuffer, &record_type));
        EXPECT_EQUAL(record_type, 0xf3);
        EXPECT_EQUAL(s2n_stuffer_data_available(&plaintext_stuffer), 1);

        /* Clean up */
        EXPECT_SUCCESS(s2n_stuffer_free(&plaintext_stuffer));
    }

    END_TEST();

}

