/* Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved. */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file s2n_socket_quickack_harness.c
 * @brief Implements the proof harness for s2n_socket_quickack function.
 */

#include <utils/s2n_socket.h>

void s2n_socket_quickack_harness()
{
  struct s2n_socket_read_io_context *cbmc_allocate_s2n_socket_read_io_context();
  /* Non-deterministic inputs. */
  struct s2n_connection *s2n_connection = malloc(sizeof(*s2n_connection));
  if (s2n_connection != NULL) {
    s2n_connection->recv_io_context = cbmc_allocate_s2n_socket_read_io_context();
  }
  
  /* Operation under verification. */
  int result = s2n_socket_quickack(s2n_connection);

  /* Post-condition. */
  assert(S2N_IMPLIES(result == S2N_SUCCESS, (!s2n_connection->managed_io) || (s2n_connection->recv_io_context) != NULL));
}
