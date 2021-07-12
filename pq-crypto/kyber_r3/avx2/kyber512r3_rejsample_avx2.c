#include <string.h>
#include "../kyber512r3_params.h"
#include "kyber512r3_consts_avx2.h"
#include "kyber512r3_rejsample_avx2.h"
#include <immintrin.h>

static const uint8_t idx[256][8] = {
  {-1, -1, -1, -1, -1, -1, -1, -1},
  { 0, -1, -1, -1, -1, -1, -1, -1},
  { 2, -1, -1, -1, -1, -1, -1, -1},
  { 0,  2, -1, -1, -1, -1, -1, -1},
  { 4, -1, -1, -1, -1, -1, -1, -1},
  { 0,  4, -1, -1, -1, -1, -1, -1},
  { 2,  4, -1, -1, -1, -1, -1, -1},
  { 0,  2,  4, -1, -1, -1, -1, -1},
  { 6, -1, -1, -1, -1, -1, -1, -1},
  { 0,  6, -1, -1, -1, -1, -1, -1},
  { 2,  6, -1, -1, -1, -1, -1, -1},
  { 0,  2,  6, -1, -1, -1, -1, -1},
  { 4,  6, -1, -1, -1, -1, -1, -1},
  { 0,  4,  6, -1, -1, -1, -1, -1},
  { 2,  4,  6, -1, -1, -1, -1, -1},
  { 0,  2,  4,  6, -1, -1, -1, -1},
  { 8, -1, -1, -1, -1, -1, -1, -1},
  { 0,  8, -1, -1, -1, -1, -1, -1},
  { 2,  8, -1, -1, -1, -1, -1, -1},
  { 0,  2,  8, -1, -1, -1, -1, -1},
  { 4,  8, -1, -1, -1, -1, -1, -1},
  { 0,  4,  8, -1, -1, -1, -1, -1},
  { 2,  4,  8, -1, -1, -1, -1, -1},
  { 0,  2,  4,  8, -1, -1, -1, -1},
  { 6,  8, -1, -1, -1, -1, -1, -1},
  { 0,  6,  8, -1, -1, -1, -1, -1},
  { 2,  6,  8, -1, -1, -1, -1, -1},
  { 0,  2,  6,  8, -1, -1, -1, -1},
  { 4,  6,  8, -1, -1, -1, -1, -1},
  { 0,  4,  6,  8, -1, -1, -1, -1},
  { 2,  4,  6,  8, -1, -1, -1, -1},
  { 0,  2,  4,  6,  8, -1, -1, -1},
  {10, -1, -1, -1, -1, -1, -1, -1},
  { 0, 10, -1, -1, -1, -1, -1, -1},
  { 2, 10, -1, -1, -1, -1, -1, -1},
  { 0,  2, 10, -1, -1, -1, -1, -1},
  { 4, 10, -1, -1, -1, -1, -1, -1},
  { 0,  4, 10, -1, -1, -1, -1, -1},
  { 2,  4, 10, -1, -1, -1, -1, -1},
  { 0,  2,  4, 10, -1, -1, -1, -1},
  { 6, 10, -1, -1, -1, -1, -1, -1},
  { 0,  6, 10, -1, -1, -1, -1, -1},
  { 2,  6, 10, -1, -1, -1, -1, -1},
  { 0,  2,  6, 10, -1, -1, -1, -1},
  { 4,  6, 10, -1, -1, -1, -1, -1},
  { 0,  4,  6, 10, -1, -1, -1, -1},
  { 2,  4,  6, 10, -1, -1, -1, -1},
  { 0,  2,  4,  6, 10, -1, -1, -1},
  { 8, 10, -1, -1, -1, -1, -1, -1},
  { 0,  8, 10, -1, -1, -1, -1, -1},
  { 2,  8, 10, -1, -1, -1, -1, -1},
  { 0,  2,  8, 10, -1, -1, -1, -1},
  { 4,  8, 10, -1, -1, -1, -1, -1},
  { 0,  4,  8, 10, -1, -1, -1, -1},
  { 2,  4,  8, 10, -1, -1, -1, -1},
  { 0,  2,  4,  8, 10, -1, -1, -1},
  { 6,  8, 10, -1, -1, -1, -1, -1},
  { 0,  6,  8, 10, -1, -1, -1, -1},
  { 2,  6,  8, 10, -1, -1, -1, -1},
  { 0,  2,  6,  8, 10, -1, -1, -1},
  { 4,  6,  8, 10, -1, -1, -1, -1},
  { 0,  4,  6,  8, 10, -1, -1, -1},
  { 2,  4,  6,  8, 10, -1, -1, -1},
  { 0,  2,  4,  6,  8, 10, -1, -1},
  {12, -1, -1, -1, -1, -1, -1, -1},
  { 0, 12, -1, -1, -1, -1, -1, -1},
  { 2, 12, -1, -1, -1, -1, -1, -1},
  { 0,  2, 12, -1, -1, -1, -1, -1},
  { 4, 12, -1, -1, -1, -1, -1, -1},
  { 0,  4, 12, -1, -1, -1, -1, -1},
  { 2,  4, 12, -1, -1, -1, -1, -1},
  { 0,  2,  4, 12, -1, -1, -1, -1},
  { 6, 12, -1, -1, -1, -1, -1, -1},
  { 0,  6, 12, -1, -1, -1, -1, -1},
  { 2,  6, 12, -1, -1, -1, -1, -1},
  { 0,  2,  6, 12, -1, -1, -1, -1},
  { 4,  6, 12, -1, -1, -1, -1, -1},
  { 0,  4,  6, 12, -1, -1, -1, -1},
  { 2,  4,  6, 12, -1, -1, -1, -1},
  { 0,  2,  4,  6, 12, -1, -1, -1},
  { 8, 12, -1, -1, -1, -1, -1, -1},
  { 0,  8, 12, -1, -1, -1, -1, -1},
  { 2,  8, 12, -1, -1, -1, -1, -1},
  { 0,  2,  8, 12, -1, -1, -1, -1},
  { 4,  8, 12, -1, -1, -1, -1, -1},
  { 0,  4,  8, 12, -1, -1, -1, -1},
  { 2,  4,  8, 12, -1, -1, -1, -1},
  { 0,  2,  4,  8, 12, -1, -1, -1},
  { 6,  8, 12, -1, -1, -1, -1, -1},
  { 0,  6,  8, 12, -1, -1, -1, -1},
  { 2,  6,  8, 12, -1, -1, -1, -1},
  { 0,  2,  6,  8, 12, -1, -1, -1},
  { 4,  6,  8, 12, -1, -1, -1, -1},
  { 0,  4,  6,  8, 12, -1, -1, -1},
  { 2,  4,  6,  8, 12, -1, -1, -1},
  { 0,  2,  4,  6,  8, 12, -1, -1},
  {10, 12, -1, -1, -1, -1, -1, -1},
  { 0, 10, 12, -1, -1, -1, -1, -1},
  { 2, 10, 12, -1, -1, -1, -1, -1},
  { 0,  2, 10, 12, -1, -1, -1, -1},
  { 4, 10, 12, -1, -1, -1, -1, -1},
  { 0,  4, 10, 12, -1, -1, -1, -1},
  { 2,  4, 10, 12, -1, -1, -1, -1},
  { 0,  2,  4, 10, 12, -1, -1, -1},
  { 6, 10, 12, -1, -1, -1, -1, -1},
  { 0,  6, 10, 12, -1, -1, -1, -1},
  { 2,  6, 10, 12, -1, -1, -1, -1},
  { 0,  2,  6, 10, 12, -1, -1, -1},
  { 4,  6, 10, 12, -1, -1, -1, -1},
  { 0,  4,  6, 10, 12, -1, -1, -1},
  { 2,  4,  6, 10, 12, -1, -1, -1},
  { 0,  2,  4,  6, 10, 12, -1, -1},
  { 8, 10, 12, -1, -1, -1, -1, -1},
  { 0,  8, 10, 12, -1, -1, -1, -1},
  { 2,  8, 10, 12, -1, -1, -1, -1},
  { 0,  2,  8, 10, 12, -1, -1, -1},
  { 4,  8, 10, 12, -1, -1, -1, -1},
  { 0,  4,  8, 10, 12, -1, -1, -1},
  { 2,  4,  8, 10, 12, -1, -1, -1},
  { 0,  2,  4,  8, 10, 12, -1, -1},
  { 6,  8, 10, 12, -1, -1, -1, -1},
  { 0,  6,  8, 10, 12, -1, -1, -1},
  { 2,  6,  8, 10, 12, -1, -1, -1},
  { 0,  2,  6,  8, 10, 12, -1, -1},
  { 4,  6,  8, 10, 12, -1, -1, -1},
  { 0,  4,  6,  8, 10, 12, -1, -1},
  { 2,  4,  6,  8, 10, 12, -1, -1},
  { 0,  2,  4,  6,  8, 10, 12, -1},
  {14, -1, -1, -1, -1, -1, -1, -1},
  { 0, 14, -1, -1, -1, -1, -1, -1},
  { 2, 14, -1, -1, -1, -1, -1, -1},
  { 0,  2, 14, -1, -1, -1, -1, -1},
  { 4, 14, -1, -1, -1, -1, -1, -1},
  { 0,  4, 14, -1, -1, -1, -1, -1},
  { 2,  4, 14, -1, -1, -1, -1, -1},
  { 0,  2,  4, 14, -1, -1, -1, -1},
  { 6, 14, -1, -1, -1, -1, -1, -1},
  { 0,  6, 14, -1, -1, -1, -1, -1},
  { 2,  6, 14, -1, -1, -1, -1, -1},
  { 0,  2,  6, 14, -1, -1, -1, -1},
  { 4,  6, 14, -1, -1, -1, -1, -1},
  { 0,  4,  6, 14, -1, -1, -1, -1},
  { 2,  4,  6, 14, -1, -1, -1, -1},
  { 0,  2,  4,  6, 14, -1, -1, -1},
  { 8, 14, -1, -1, -1, -1, -1, -1},
  { 0,  8, 14, -1, -1, -1, -1, -1},
  { 2,  8, 14, -1, -1, -1, -1, -1},
  { 0,  2,  8, 14, -1, -1, -1, -1},
  { 4,  8, 14, -1, -1, -1, -1, -1},
  { 0,  4,  8, 14, -1, -1, -1, -1},
  { 2,  4,  8, 14, -1, -1, -1, -1},
  { 0,  2,  4,  8, 14, -1, -1, -1},
  { 6,  8, 14, -1, -1, -1, -1, -1},
  { 0,  6,  8, 14, -1, -1, -1, -1},
  { 2,  6,  8, 14, -1, -1, -1, -1},
  { 0,  2,  6,  8, 14, -1, -1, -1},
  { 4,  6,  8, 14, -1, -1, -1, -1},
  { 0,  4,  6,  8, 14, -1, -1, -1},
  { 2,  4,  6,  8, 14, -1, -1, -1},
  { 0,  2,  4,  6,  8, 14, -1, -1},
  {10, 14, -1, -1, -1, -1, -1, -1},
  { 0, 10, 14, -1, -1, -1, -1, -1},
  { 2, 10, 14, -1, -1, -1, -1, -1},
  { 0,  2, 10, 14, -1, -1, -1, -1},
  { 4, 10, 14, -1, -1, -1, -1, -1},
  { 0,  4, 10, 14, -1, -1, -1, -1},
  { 2,  4, 10, 14, -1, -1, -1, -1},
  { 0,  2,  4, 10, 14, -1, -1, -1},
  { 6, 10, 14, -1, -1, -1, -1, -1},
  { 0,  6, 10, 14, -1, -1, -1, -1},
  { 2,  6, 10, 14, -1, -1, -1, -1},
  { 0,  2,  6, 10, 14, -1, -1, -1},
  { 4,  6, 10, 14, -1, -1, -1, -1},
  { 0,  4,  6, 10, 14, -1, -1, -1},
  { 2,  4,  6, 10, 14, -1, -1, -1},
  { 0,  2,  4,  6, 10, 14, -1, -1},
  { 8, 10, 14, -1, -1, -1, -1, -1},
  { 0,  8, 10, 14, -1, -1, -1, -1},
  { 2,  8, 10, 14, -1, -1, -1, -1},
  { 0,  2,  8, 10, 14, -1, -1, -1},
  { 4,  8, 10, 14, -1, -1, -1, -1},
  { 0,  4,  8, 10, 14, -1, -1, -1},
  { 2,  4,  8, 10, 14, -1, -1, -1},
  { 0,  2,  4,  8, 10, 14, -1, -1},
  { 6,  8, 10, 14, -1, -1, -1, -1},
  { 0,  6,  8, 10, 14, -1, -1, -1},
  { 2,  6,  8, 10, 14, -1, -1, -1},
  { 0,  2,  6,  8, 10, 14, -1, -1},
  { 4,  6,  8, 10, 14, -1, -1, -1},
  { 0,  4,  6,  8, 10, 14, -1, -1},
  { 2,  4,  6,  8, 10, 14, -1, -1},
  { 0,  2,  4,  6,  8, 10, 14, -1},
  {12, 14, -1, -1, -1, -1, -1, -1},
  { 0, 12, 14, -1, -1, -1, -1, -1},
  { 2, 12, 14, -1, -1, -1, -1, -1},
  { 0,  2, 12, 14, -1, -1, -1, -1},
  { 4, 12, 14, -1, -1, -1, -1, -1},
  { 0,  4, 12, 14, -1, -1, -1, -1},
  { 2,  4, 12, 14, -1, -1, -1, -1},
  { 0,  2,  4, 12, 14, -1, -1, -1},
  { 6, 12, 14, -1, -1, -1, -1, -1},
  { 0,  6, 12, 14, -1, -1, -1, -1},
  { 2,  6, 12, 14, -1, -1, -1, -1},
  { 0,  2,  6, 12, 14, -1, -1, -1},
  { 4,  6, 12, 14, -1, -1, -1, -1},
  { 0,  4,  6, 12, 14, -1, -1, -1},
  { 2,  4,  6, 12, 14, -1, -1, -1},
  { 0,  2,  4,  6, 12, 14, -1, -1},
  { 8, 12, 14, -1, -1, -1, -1, -1},
  { 0,  8, 12, 14, -1, -1, -1, -1},
  { 2,  8, 12, 14, -1, -1, -1, -1},
  { 0,  2,  8, 12, 14, -1, -1, -1},
  { 4,  8, 12, 14, -1, -1, -1, -1},
  { 0,  4,  8, 12, 14, -1, -1, -1},
  { 2,  4,  8, 12, 14, -1, -1, -1},
  { 0,  2,  4,  8, 12, 14, -1, -1},
  { 6,  8, 12, 14, -1, -1, -1, -1},
  { 0,  6,  8, 12, 14, -1, -1, -1},
  { 2,  6,  8, 12, 14, -1, -1, -1},
  { 0,  2,  6,  8, 12, 14, -1, -1},
  { 4,  6,  8, 12, 14, -1, -1, -1},
  { 0,  4,  6,  8, 12, 14, -1, -1},
  { 2,  4,  6,  8, 12, 14, -1, -1},
  { 0,  2,  4,  6,  8, 12, 14, -1},
  {10, 12, 14, -1, -1, -1, -1, -1},
  { 0, 10, 12, 14, -1, -1, -1, -1},
  { 2, 10, 12, 14, -1, -1, -1, -1},
  { 0,  2, 10, 12, 14, -1, -1, -1},
  { 4, 10, 12, 14, -1, -1, -1, -1},
  { 0,  4, 10, 12, 14, -1, -1, -1},
  { 2,  4, 10, 12, 14, -1, -1, -1},
  { 0,  2,  4, 10, 12, 14, -1, -1},
  { 6, 10, 12, 14, -1, -1, -1, -1},
  { 0,  6, 10, 12, 14, -1, -1, -1},
  { 2,  6, 10, 12, 14, -1, -1, -1},
  { 0,  2,  6, 10, 12, 14, -1, -1},
  { 4,  6, 10, 12, 14, -1, -1, -1},
  { 0,  4,  6, 10, 12, 14, -1, -1},
  { 2,  4,  6, 10, 12, 14, -1, -1},
  { 0,  2,  4,  6, 10, 12, 14, -1},
  { 8, 10, 12, 14, -1, -1, -1, -1},
  { 0,  8, 10, 12, 14, -1, -1, -1},
  { 2,  8, 10, 12, 14, -1, -1, -1},
  { 0,  2,  8, 10, 12, 14, -1, -1},
  { 4,  8, 10, 12, 14, -1, -1, -1},
  { 0,  4,  8, 10, 12, 14, -1, -1},
  { 2,  4,  8, 10, 12, 14, -1, -1},
  { 0,  2,  4,  8, 10, 12, 14, -1},
  { 6,  8, 10, 12, 14, -1, -1, -1},
  { 0,  6,  8, 10, 12, 14, -1, -1},
  { 2,  6,  8, 10, 12, 14, -1, -1},
  { 0,  2,  6,  8, 10, 12, 14, -1},
  { 4,  6,  8, 10, 12, 14, -1, -1},
  { 0,  4,  6,  8, 10, 12, 14, -1},
  { 2,  4,  6,  8, 10, 12, 14, -1},
  { 0,  2,  4,  6,  8, 10, 12, 14}
};

unsigned int rej_uniform_avx2(int16_t * restrict r,
                             const uint8_t * restrict buf)
{
  unsigned int ctr, pos;
  uint16_t val0, val1;
  uint32_t good;
  const __m256i bound  = _mm256_set1_epi16(S2N_KYBER_512_R3_Q);
  const __m256i ones   = _mm256_set1_epi8(1);
  const __m256i mask  = _mm256_set1_epi16(0xFFF);
  const __m256i idx8  = _mm256_set_epi8(15,14,14,13,12,11,11,10,
                                         9, 8, 8, 7, 6, 5, 5, 4,
                                        11,10,10, 9, 8, 7, 7, 6,
                                         5, 4, 4, 3, 2, 1, 1, 0);
  __m256i f0, f1, g0, g1, g2, g3;
  __m128i f, t, pilo, pihi;

  ctr = pos = 0;
  while(ctr <= S2N_KYBER_512_R3_N - 32 && pos <= AVX_REJ_UNIFORM_BUFLEN - 48) {
    f0 = _mm256_loadu_si256((const void *)&buf[pos]);
    f1 = _mm256_loadu_si256((const void *)&buf[pos+24]);
    f0 = _mm256_permute4x64_epi64(f0, 0x94);
    f1 = _mm256_permute4x64_epi64(f1, 0x94);
    f0 = _mm256_shuffle_epi8(f0, idx8);
    f1 = _mm256_shuffle_epi8(f1, idx8);
    g0 = _mm256_srli_epi16(f0, 4);
    g1 = _mm256_srli_epi16(f1, 4);
    f0 = _mm256_blend_epi16(f0, g0, 0xAA);
    f1 = _mm256_blend_epi16(f1, g1, 0xAA);
    f0 = _mm256_and_si256(f0, mask);
    f1 = _mm256_and_si256(f1, mask);
    pos += 48;

    g0 = _mm256_cmpgt_epi16(bound, f0);
    g1 = _mm256_cmpgt_epi16(bound, f1);

    g0 = _mm256_packs_epi16(g0, g1);
    good = _mm256_movemask_epi8(g0);

    g0 = _mm256_castsi128_si256(_mm_loadl_epi64((__m128i *)&idx[(good >>  0) & 0xFF]));
    g1 = _mm256_castsi128_si256(_mm_loadl_epi64((__m128i *)&idx[(good >>  8) & 0xFF]));
    g0 = _mm256_inserti128_si256(g0, _mm_loadl_epi64((__m128i *)&idx[(good >> 16) & 0xFF]), 1);
    g1 = _mm256_inserti128_si256(g1, _mm_loadl_epi64((__m128i *)&idx[(good >> 24) & 0xFF]), 1);

    g2 = _mm256_add_epi8(g0, ones);
    g3 = _mm256_add_epi8(g1, ones);
    g0 = _mm256_unpacklo_epi8(g0, g2);
    g1 = _mm256_unpacklo_epi8(g1, g3);

    f0 = _mm256_shuffle_epi8(f0, g0);
    f1 = _mm256_shuffle_epi8(f1, g1);

    _mm_storeu_si128((__m128i *)&r[ctr], _mm256_castsi256_si128(f0));
    ctr += _mm_popcnt_u32((good >>  0) & 0xFF);
    _mm_storeu_si128((__m128i *)&r[ctr], _mm256_extracti128_si256(f0, 1));
    ctr += _mm_popcnt_u32((good >> 16) & 0xFF);
    _mm_storeu_si128((__m128i *)&r[ctr], _mm256_castsi256_si128(f1));
    ctr += _mm_popcnt_u32((good >>  8) & 0xFF);
    _mm_storeu_si128((__m128i *)&r[ctr], _mm256_extracti128_si256(f1, 1));
    ctr += _mm_popcnt_u32((good >> 24) & 0xFF);
  }

  while(ctr <= S2N_KYBER_512_R3_N - 8 && pos <= AVX_REJ_UNIFORM_BUFLEN - 12) {
    f = _mm_loadu_si128((const void *)&buf[pos]);
    f = _mm_shuffle_epi8(f, _mm256_castsi256_si128(idx8));
    t = _mm_srli_epi16(f, 4);
    f = _mm_blend_epi16(f, t, 0xAA);
    f = _mm_and_si128(f, _mm256_castsi256_si128(mask));
    pos += 12;

    t = _mm_cmpgt_epi16(_mm256_castsi256_si128(bound), f);
    good = _mm_movemask_epi8(t);

    good = _pext_u32(good, 0x5555);
    pilo = _mm_loadl_epi64((__m128i *)&idx[good]);

    pihi = _mm_add_epi8(pilo, _mm256_castsi256_si128(ones));
    pilo = _mm_unpacklo_epi8(pilo, pihi);
    f = _mm_shuffle_epi8(f, pilo);
    _mm_storeu_si128((__m128i *)&r[ctr], f);
    ctr += _mm_popcnt_u32(good);
  }

  while(ctr < S2N_KYBER_512_R3_N && pos <= AVX_REJ_UNIFORM_BUFLEN - 3) {
    val0 = ((buf[pos+0] >> 0) | ((uint16_t)buf[pos+1] << 8)) & 0xFFF;
    val1 = ((buf[pos+1] >> 4) | ((uint16_t)buf[pos+2] << 4));
    pos += 3;

    if(val0 < S2N_KYBER_512_R3_Q)
      r[ctr++] = val0;
    if(val1 < S2N_KYBER_512_R3_Q && ctr < S2N_KYBER_512_R3_N)
      r[ctr++] = val1;
  }

  return ctr;
}
