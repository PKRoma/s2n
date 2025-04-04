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

#include "crypto/s2n_rsa_pss.h"

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <stdint.h>

#include "crypto/s2n_hash.h"
#include "crypto/s2n_pkey.h"
#include "crypto/s2n_pkey_evp.h"
#include "crypto/s2n_rsa.h"
#include "crypto/s2n_rsa_signing.h"
#include "error/s2n_errno.h"
#include "stuffer/s2n_stuffer.h"
#include "utils/s2n_blob.h"
#include "utils/s2n_random.h"
#include "utils/s2n_safety.h"

/* Checks whether PSS Certs is supported */
int s2n_is_rsa_pss_certs_supported()
{
    return RSA_PSS_CERTS_SUPPORTED;
}

#if RSA_PSS_CERTS_SUPPORTED

static S2N_RESULT s2n_rsa_pss_size(const struct s2n_pkey *key, uint32_t *size_out)
{
    RESULT_ENSURE_REF(key);
    RESULT_ENSURE_REF(size_out);

    /* For more info, see: https://www.openssl.org/docs/man1.1.0/man3/EVP_PKEY_size.html */
    const int size = EVP_PKEY_size(key->pkey);
    RESULT_GUARD_POSIX(size);
    *size_out = size;

    return S2N_RESULT_OK;
}

static int s2n_rsa_is_private_key(const RSA *rsa_key)
{
    const BIGNUM *d = NULL;
    RSA_get0_key(rsa_key, NULL, NULL, &d);

    if (d != NULL) {
        return 1;
    }
    return 0;
}

int s2n_rsa_pss_key_sign(const struct s2n_pkey *priv, s2n_signature_algorithm sig_alg,
        struct s2n_hash_state *digest, struct s2n_blob *signature_out)
{
    POSIX_ENSURE_REF(priv);
    sig_alg_check(sig_alg, S2N_SIGNATURE_RSA_PSS_PSS);

    /* Not Possible to Sign with Public Key */
    const RSA *key = priv->key.rsa_key.rsa;
    POSIX_ENSURE(s2n_rsa_is_private_key(key), S2N_ERR_KEY_MISMATCH);

    return s2n_rsa_pss_sign(priv, digest, signature_out);
}

int s2n_rsa_pss_key_verify(const struct s2n_pkey *pub, s2n_signature_algorithm sig_alg,
        struct s2n_hash_state *digest, struct s2n_blob *signature_in)
{
    POSIX_ENSURE_REF(pub);
    sig_alg_check(sig_alg, S2N_SIGNATURE_RSA_PSS_PSS);

    /* Using Private Key to Verify means the public/private keys were likely swapped, and likely indicates a bug. */
    const RSA *key = pub->key.rsa_key.rsa;
    POSIX_ENSURE(!s2n_rsa_is_private_key(key), S2N_ERR_KEY_MISMATCH);

    return s2n_rsa_pss_verify(pub, digest, signature_in);
}

static int s2n_rsa_pss_key_free(struct s2n_pkey *pkey)
{
    POSIX_ENSURE_REF(pkey);
    struct s2n_rsa_key *rsa_key = &pkey->key.rsa_key;
    if (rsa_key->rsa == NULL) {
        return S2N_SUCCESS;
    }

    /* Safety: freeing the key owned by this object */
    RSA_free(s2n_unsafe_rsa_get_non_const(rsa_key));
    rsa_key->rsa = NULL;

    return S2N_SUCCESS;
}

S2N_RESULT s2n_evp_pkey_to_rsa_pss_public_key(struct s2n_rsa_key *rsa_key, EVP_PKEY *pkey)
{
    const RSA *pub_rsa_key = EVP_PKEY_get1_RSA(pkey);
    RESULT_ENSURE_REF(pub_rsa_key);

    RESULT_ENSURE(!s2n_rsa_is_private_key(pub_rsa_key), S2N_ERR_KEY_MISMATCH);

    rsa_key->rsa = pub_rsa_key;
    return S2N_RESULT_OK;
}

S2N_RESULT s2n_evp_pkey_to_rsa_pss_private_key(struct s2n_rsa_key *rsa_key, EVP_PKEY *pkey)
{
    const RSA *priv_rsa_key = EVP_PKEY_get1_RSA(pkey);
    RESULT_ENSURE_REF(priv_rsa_key);

    /* Documentation: https://www.openssl.org/docs/man1.1.1/man3/RSA_check_key.html */
    RESULT_ENSURE(s2n_rsa_is_private_key(priv_rsa_key), S2N_ERR_KEY_MISMATCH);

    /* Check that the mandatory properties of a RSA Private Key are valid.
     *  - Documentation: https://www.openssl.org/docs/man1.1.1/man3/RSA_check_key.html
     */
    RESULT_GUARD_OSSL(RSA_check_key(priv_rsa_key), S2N_ERR_KEY_CHECK);

    rsa_key->rsa = priv_rsa_key;
    return S2N_RESULT_OK;
}

S2N_RESULT s2n_rsa_pss_pkey_init(struct s2n_pkey *pkey)
{
    RESULT_GUARD(s2n_rsa_pkey_init(pkey));

    pkey->size = &s2n_rsa_pss_size;
    pkey->sign = &s2n_rsa_pss_key_sign;
    pkey->verify = &s2n_rsa_pss_key_verify;

    /* RSA PSS only supports Sign and Verify.
     * RSA PSS should never be used for Key Exchange. ECDHE should be used instead since it provides Forward Secrecy. */
    pkey->encrypt = NULL; /* No function for encryption */
    pkey->decrypt = NULL; /* No function for decryption */

    pkey->free = &s2n_rsa_pss_key_free;

    RESULT_GUARD(s2n_pkey_evp_set_overrides(pkey));
    return S2N_RESULT_OK;
}

#else

S2N_RESULT s2n_evp_pkey_to_rsa_pss_public_key(struct s2n_rsa_key *rsa_pss_key, EVP_PKEY *pkey)
{
    RESULT_BAIL(S2N_ERR_RSA_PSS_NOT_SUPPORTED);
}

S2N_RESULT s2n_evp_pkey_to_rsa_pss_private_key(struct s2n_rsa_key *rsa_pss_key, EVP_PKEY *pkey)
{
    RESULT_BAIL(S2N_ERR_RSA_PSS_NOT_SUPPORTED);
}

S2N_RESULT s2n_rsa_pss_pkey_init(struct s2n_pkey *pkey)
{
    RESULT_BAIL(S2N_ERR_RSA_PSS_NOT_SUPPORTED);
}

#endif
