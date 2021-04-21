#include <stddef.h>
#include <stdint.h>
#include "params.h"
#include "symmetric.h"
#include "verify.h"
#include "indcpa.h"
#include "tls/s2n_kem.h"
#include "utils/s2n_safety.h"
#include "pq-crypto/s2n_pq_random.h"
#include "pq-crypto/s2n_pq.h"

/*************************************************
* Name:        crypto_kem_keypair
*
* Description: Generates public and private key
*              for CCA-secure Kyber key encapsulation mechanism
*
* Arguments:   - unsigned char *pk: pointer to output public key
*                (an already allocated array of KYBER_512_R3_PUBLIC_KEY_BYTES bytes)
*              - unsigned char *sk: pointer to output private key
*                (an already allocated array of KYBER_512_R3_SECRET_KEY_BYTES bytes)
*
* Returns 0 (success)
**************************************************/
int kyber_512_r3_crypto_kem_keypair(unsigned char *pk, unsigned char *sk)
{
    POSIX_ENSURE(s2n_pq_is_enabled(), S2N_ERR_PQ_DISABLED);
    POSIX_GUARD(indcpa_keypair(pk, sk));
    for(size_t i = 0; i < KYBER_INDCPA_PUBLICKEYBYTES; i++) {
        sk[i + KYBER_INDCPA_SECRETKEYBYTES] = pk[i];
    }
    hash_h(sk+KYBER_SECRETKEYBYTES-2*KYBER_SYMBYTES, pk, KYBER_PUBLICKEYBYTES);
    /* Value z for pseudo-random output on reject */
    POSIX_GUARD_RESULT(s2n_get_random_bytes(sk+KYBER_SECRETKEYBYTES-KYBER_SYMBYTES, KYBER_SYMBYTES));
    return S2N_SUCCESS;
}

/*************************************************
* Name:        crypto_kem_enc
*
* Description: Generates cipher text and shared
*              secret for given public key
*
* Arguments:   - unsigned char *ct: pointer to output cipher text
*                (an already allocated array of KYBER_512_R3_CIPHERTEXT_BYTES bytes)
*              - unsigned char *ss: pointer to output shared secret
*                (an already allocated array of KYBER_512_R3_SHARED_SECRET_BYTES bytes)
*              - const unsigned char *pk: pointer to input public key
*                (an already allocated array of KYBER_512_R3_PUBLIC_KEY_BYTES bytes)
*
* Returns 0 (success)
**************************************************/
int kyber_512_r3_crypto_kem_enc(unsigned char *ct, unsigned char *ss, const unsigned char *pk)
{
    POSIX_ENSURE(s2n_pq_is_enabled(), S2N_ERR_PQ_DISABLED);
    uint8_t buf[2*KYBER_SYMBYTES];
    /* Will contain key, coins */
    uint8_t kr[2*KYBER_SYMBYTES];

    POSIX_GUARD_RESULT(s2n_get_random_bytes(buf, KYBER_SYMBYTES));
    /* Don't release system RNG output */
    hash_h(buf, buf, KYBER_SYMBYTES);

    /* Multitarget countermeasure for coins + contributory KEM */
    hash_h(buf+KYBER_SYMBYTES, pk, KYBER_PUBLICKEYBYTES);
    hash_g(kr, buf, 2*KYBER_SYMBYTES);

    /* coins are in kr+KYBER_SYMBYTES */
    indcpa_enc(ct, buf, pk, kr+KYBER_SYMBYTES);

    /* overwrite coins in kr with H(c) */
    hash_h(kr+KYBER_SYMBYTES, ct, KYBER_CIPHERTEXTBYTES);
    /* hash concatenation of pre-k and H(c) to k */
    kdf(ss, kr, 2*KYBER_SYMBYTES);
    return S2N_SUCCESS;
}

/*************************************************
* Name:        crypto_kem_dec
*
* Description: Generates shared secret for given
*              cipher text and private key
*
* Arguments:   - unsigned char *ss: pointer to output shared secret
*                (an already allocated array of KYBER_512_R3_SHARED_SECRET_BYTES bytes)
*              - const unsigned char *ct: pointer to input cipher text
*                (an already allocated array of KYBER_512_R3_CIPHERTEXT_BYTES bytes)
*              - const unsigned char *sk: pointer to input private key
*                (an already allocated array of KYBER_512_R3_SECRET_KEY_BYTES bytes)
*
* Returns 0.
*
* On failure, ss will contain a pseudo-random value.
**************************************************/
int kyber_512_r3_crypto_kem_dec(unsigned char *ss, const unsigned char *ct, const unsigned char *sk)
{
    POSIX_ENSURE(s2n_pq_is_enabled(), S2N_ERR_PQ_DISABLED);
    int fail;
    uint8_t buf[2*KYBER_SYMBYTES];
    /* Will contain key, coins */
    uint8_t kr[2*KYBER_SYMBYTES];
    uint8_t cmp[KYBER_CIPHERTEXTBYTES];
    const uint8_t *pk = sk+KYBER_INDCPA_SECRETKEYBYTES;

    indcpa_dec(buf, ct, sk);

    /* Multitarget countermeasure for coins + contributory KEM */
    for(size_t i = 0; i < KYBER_SYMBYTES; i++) {
        buf[KYBER_SYMBYTES + i] = sk[KYBER_SECRETKEYBYTES - 2 * KYBER_SYMBYTES + i];
    }
    hash_g(kr, buf, 2*KYBER_SYMBYTES);

    /* coins are in kr+KYBER_SYMBYTES */
    indcpa_enc(cmp, buf, pk, kr+KYBER_SYMBYTES);

    fail = verify(ct, cmp, KYBER_CIPHERTEXTBYTES);

    /* overwrite coins in kr with H(c) */
    hash_h(kr+KYBER_SYMBYTES, ct, KYBER_CIPHERTEXTBYTES);

    /* Overwrite pre-k with z on re-encryption failure */
    cmov(kr, sk+KYBER_SECRETKEYBYTES-KYBER_SYMBYTES, KYBER_SYMBYTES, fail);

    /* hash concatenation of pre-k and H(c) to k */
    kdf(ss, kr, 2*KYBER_SYMBYTES);
    return S2N_SUCCESS;
}
