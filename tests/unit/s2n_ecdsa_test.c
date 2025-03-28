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

#include "crypto/s2n_ecdsa.h"

#include <string.h>

#include "crypto/s2n_ecc_evp.h"
#include "crypto/s2n_fips.h"
#include "s2n_test.h"
#include "stuffer/s2n_stuffer.h"
#include "testlib/s2n_testlib.h"
#include "tls/s2n_config.h"
#include "tls/s2n_connection.h"
#include "utils/s2n_safety.h"

static uint8_t s2n_test_noop_verify_host_fn(const char *host_name, size_t host_name_len, void *data)
{
    return true;
}

static uint8_t unmatched_private_key[] =
        "-----BEGIN EC PRIVATE KEY-----\n"
        "MIIB+gIBAQQwuenHFMJsDm5tCQgthH8kGXQ1dHkKACmHH3ZqIGteoghhGow6vGmr\n"
        "xzA8gAdD2bJ0oIIBWzCCAVcCAQEwPAYHKoZIzj0BAQIxAP//////////////////\n"
        "///////////////////////+/////wAAAAAAAAAA/////zB7BDD/////////////\n"
        "/////////////////////////////v////8AAAAAAAAAAP////wEMLMxL6fiPufk\n"
        "mI4Fa+P4LRkYHZxu/oFBEgMUCI9QE4daxlY5jYou0Z0qhcjt0+wq7wMVAKM1kmqj\n"
        "GaJ6HQCJamdzpIJ6zaxzBGEEqofKIr6LBTeOscce8yCtdG4dO2KLp5uYWfdB4IJU\n"
        "KjhVAvJdv1UpbDpUXjhydgq3NhfeSpYmLG9dnpi/kpLcKfj0Hb0omhR86doxE7Xw\n"
        "uMAKYLHOHX6BnXpDHXyQ6g5fAjEA////////////////////////////////x2NN\n"
        "gfQ3Ld9YGg2ySLCneuzsGWrMxSlzAgEBoWQDYgAE8oYPSRINnKlr5ZBHWacYEq4Y\n"
        "j18l5f9yoMhBhpl7qvzf7uNFQ1SHzgHu0/v662d8Z0Pc0ujIms3/9uYxXVUY73vm\n"
        "iwVevOxBJ1GL0usqhWNqOKoNp048H4rCmfyMN97E\n"
        "-----END EC PRIVATE KEY-----\n";

int main(int argc, char **argv)
{
    struct s2n_stuffer certificate_in = { 0 }, certificate_out = { 0 };
    struct s2n_stuffer ecdsa_key_in = { 0 }, ecdsa_key_out = { 0 };
    struct s2n_stuffer unmatched_ecdsa_key_in = { 0 }, unmatched_ecdsa_key_out = { 0 };
    struct s2n_blob b = { 0 };
    char *cert_chain_pem = NULL;
    char *private_key_pem = NULL;

    BEGIN_TEST();
    EXPECT_SUCCESS(s2n_disable_tls13_in_test());

    /* s2n_ecdsa_pkey_matches_curve */
    {
        struct s2n_ecdsa_key *p256_key = NULL, *p384_key = NULL;
        struct s2n_cert_chain_and_key *p256_chain = NULL, *p384_chain = NULL;

        EXPECT_SUCCESS(s2n_test_cert_chain_and_key_new(&p256_chain,
                S2N_ECDSA_P256_PKCS1_CERT_CHAIN, S2N_ECDSA_P256_PKCS1_KEY));
        EXPECT_SUCCESS(s2n_test_cert_chain_and_key_new(&p384_chain,
                S2N_ECDSA_P384_PKCS1_CERT_CHAIN, S2N_ECDSA_P384_PKCS1_KEY));

        p256_key = &p256_chain->private_key->key.ecdsa_key;
        p384_key = &p384_chain->private_key->key.ecdsa_key;

        EXPECT_SUCCESS(s2n_ecdsa_pkey_matches_curve(p256_key, &s2n_ecc_curve_secp256r1));
        EXPECT_SUCCESS(s2n_ecdsa_pkey_matches_curve(p384_key, &s2n_ecc_curve_secp384r1));

        EXPECT_FAILURE(s2n_ecdsa_pkey_matches_curve(p256_key, &s2n_ecc_curve_secp384r1));
        EXPECT_FAILURE(s2n_ecdsa_pkey_matches_curve(p384_key, &s2n_ecc_curve_secp256r1));

        EXPECT_SUCCESS(s2n_cert_chain_and_key_free(p256_chain));
        EXPECT_SUCCESS(s2n_cert_chain_and_key_free(p384_chain));
    };

    EXPECT_SUCCESS(s2n_stuffer_alloc(&certificate_in, S2N_MAX_TEST_PEM_SIZE));
    EXPECT_SUCCESS(s2n_stuffer_alloc(&certificate_out, S2N_MAX_TEST_PEM_SIZE));
    EXPECT_SUCCESS(s2n_stuffer_alloc(&ecdsa_key_in, S2N_MAX_TEST_PEM_SIZE));
    EXPECT_SUCCESS(s2n_stuffer_alloc(&ecdsa_key_out, S2N_MAX_TEST_PEM_SIZE));
    EXPECT_SUCCESS(s2n_stuffer_alloc(&unmatched_ecdsa_key_in, sizeof(unmatched_private_key)));
    EXPECT_SUCCESS(s2n_stuffer_alloc(&unmatched_ecdsa_key_out, sizeof(unmatched_private_key)));
    EXPECT_NOT_NULL(cert_chain_pem = malloc(S2N_MAX_TEST_PEM_SIZE));
    EXPECT_NOT_NULL(private_key_pem = malloc(S2N_MAX_TEST_PEM_SIZE));
    EXPECT_SUCCESS(s2n_read_test_pem(S2N_ECDSA_P384_PKCS1_CERT_CHAIN, cert_chain_pem, S2N_MAX_TEST_PEM_SIZE));
    EXPECT_SUCCESS(s2n_read_test_pem(S2N_ECDSA_P384_PKCS1_KEY, private_key_pem, S2N_MAX_TEST_PEM_SIZE));

    EXPECT_SUCCESS(s2n_blob_init(&b, (uint8_t *) cert_chain_pem, strlen(cert_chain_pem) + 1));
    EXPECT_SUCCESS(s2n_stuffer_write(&certificate_in, &b));

    EXPECT_SUCCESS(s2n_blob_init(&b, (uint8_t *) private_key_pem, strlen(private_key_pem) + 1));
    EXPECT_SUCCESS(s2n_stuffer_write(&ecdsa_key_in, &b));

    EXPECT_SUCCESS(s2n_blob_init(&b, (uint8_t *) unmatched_private_key, sizeof(unmatched_private_key)));
    EXPECT_SUCCESS(s2n_stuffer_write(&unmatched_ecdsa_key_in, &b));

    int type = 0;
    EXPECT_SUCCESS(s2n_stuffer_certificate_from_pem(&certificate_in, &certificate_out));
    EXPECT_SUCCESS(s2n_stuffer_private_key_from_pem(&ecdsa_key_in, &ecdsa_key_out, &type));
    EXPECT_EQUAL(type, EVP_PKEY_EC);
    EXPECT_SUCCESS(s2n_stuffer_private_key_from_pem(&unmatched_ecdsa_key_in, &unmatched_ecdsa_key_out, &type));
    EXPECT_EQUAL(type, EVP_PKEY_EC);

    struct s2n_pkey pub_key = { 0 };
    struct s2n_pkey priv_key = { 0 };
    struct s2n_pkey unmatched_priv_key = { 0 };
    s2n_pkey_type pkey_type = { 0 };
    uint32_t available_size = 0;

    available_size = s2n_stuffer_data_available(&certificate_out);
    EXPECT_SUCCESS(s2n_blob_init(&b, s2n_stuffer_raw_read(&certificate_out, available_size), available_size));
    EXPECT_OK(s2n_asn1der_to_public_key_and_type(&pub_key, &pkey_type, &b));

    /* Test without a type hint */
    int wrong_type = 0;
    EXPECT_NOT_EQUAL(wrong_type, EVP_PKEY_EC);

    available_size = s2n_stuffer_data_available(&ecdsa_key_out);
    EXPECT_SUCCESS(s2n_blob_init(&b, s2n_stuffer_raw_read(&ecdsa_key_out, available_size), available_size));
    EXPECT_OK(s2n_asn1der_to_private_key(&priv_key, &b, wrong_type));

    available_size = s2n_stuffer_data_available(&unmatched_ecdsa_key_out);
    EXPECT_SUCCESS(s2n_blob_init(&b, s2n_stuffer_raw_read(&unmatched_ecdsa_key_out, available_size), available_size));
    EXPECT_OK(s2n_asn1der_to_private_key(&unmatched_priv_key, &b, wrong_type));

    /* Verify that the public/private key pair match */
    EXPECT_SUCCESS(s2n_pkey_match(&pub_key, &priv_key));

    /* Try signing and verification with ECDSA */
    uint8_t inputpad[] = "Hello world!";
    struct s2n_blob signature = { 0 }, bad_signature = { 0 };
    struct s2n_hash_state hash_one = { 0 }, hash_two = { 0 };

    uint32_t maximum_signature_length = 0;
    EXPECT_OK(s2n_pkey_size(&priv_key, &maximum_signature_length));
    EXPECT_SUCCESS(s2n_alloc(&signature, maximum_signature_length));

    EXPECT_SUCCESS(s2n_hash_new(&hash_one));
    EXPECT_SUCCESS(s2n_hash_new(&hash_two));

    /* Determining all possible valid combinations of hash algorithms and
     * signature algorithms is actually surprisingly complicated.
     *
     * For example: awslc-fips will fail for MD5+ECDSA. However, that is not
     * a real problem because there is no valid signature scheme that uses both
     * MD5 and ECDSA.
     *
     * To avoid enumerating all the exceptions, just use the actual supported
     * signature scheme list as the source of truth.
     */
    const struct s2n_signature_preferences *all_sig_schemes =
            security_policy_test_all.signature_preferences;

    for (size_t i = 0; i < all_sig_schemes->count; i++) {
        const struct s2n_signature_scheme *scheme = all_sig_schemes->signature_schemes[i];
        if (scheme->sig_alg != S2N_SIGNATURE_ECDSA) {
            continue;
        }
        const s2n_hash_algorithm hash_alg = scheme->hash_alg;

        EXPECT_SUCCESS(s2n_hash_init(&hash_one, hash_alg));
        EXPECT_SUCCESS(s2n_hash_init(&hash_two, hash_alg));

        EXPECT_SUCCESS(s2n_hash_update(&hash_one, inputpad, sizeof(inputpad)));
        EXPECT_SUCCESS(s2n_hash_update(&hash_two, inputpad, sizeof(inputpad)));

        /* Reset signature size when we compute a new signature */
        signature.size = maximum_signature_length;

        EXPECT_SUCCESS(s2n_pkey_sign(&priv_key, S2N_SIGNATURE_ECDSA, &hash_one, &signature));
        EXPECT_SUCCESS(s2n_pkey_verify(&pub_key, S2N_SIGNATURE_ECDSA, &hash_two, &signature));

        EXPECT_SUCCESS(s2n_hash_reset(&hash_one));
        EXPECT_SUCCESS(s2n_hash_reset(&hash_two));
    }

    /* Re-initialize hashes for remaining tests */
    EXPECT_SUCCESS(s2n_hash_init(&hash_one, S2N_HASH_SHA512));
    EXPECT_SUCCESS(s2n_hash_init(&hash_two, S2N_HASH_SHA512));

    /* Mismatched public/private key should fail verification */
    EXPECT_OK(s2n_pkey_size(&unmatched_priv_key, &maximum_signature_length));
    EXPECT_SUCCESS(s2n_alloc(&bad_signature, maximum_signature_length));

    EXPECT_FAILURE(s2n_pkey_match(&pub_key, &unmatched_priv_key));

    EXPECT_SUCCESS(s2n_pkey_sign(&unmatched_priv_key, S2N_SIGNATURE_ECDSA, &hash_one, &bad_signature));
    EXPECT_FAILURE(s2n_pkey_verify(&pub_key, S2N_SIGNATURE_ECDSA, &hash_two, &bad_signature));

    EXPECT_SUCCESS(s2n_free(&signature));
    EXPECT_SUCCESS(s2n_free(&bad_signature));

    EXPECT_SUCCESS(s2n_hash_free(&hash_one));
    EXPECT_SUCCESS(s2n_hash_free(&hash_two));

    EXPECT_SUCCESS(s2n_pkey_free(&pub_key));
    EXPECT_SUCCESS(s2n_pkey_free(&priv_key));
    EXPECT_SUCCESS(s2n_pkey_free(&unmatched_priv_key));

    EXPECT_SUCCESS(s2n_stuffer_free(&certificate_in));
    EXPECT_SUCCESS(s2n_stuffer_free(&certificate_out));
    EXPECT_SUCCESS(s2n_stuffer_free(&ecdsa_key_in));
    EXPECT_SUCCESS(s2n_stuffer_free(&ecdsa_key_out));
    EXPECT_SUCCESS(s2n_stuffer_free(&unmatched_ecdsa_key_in));
    EXPECT_SUCCESS(s2n_stuffer_free(&unmatched_ecdsa_key_out));
    free(cert_chain_pem);
    free(private_key_pem);

    EXPECT_SUCCESS(s2n_reset_tls13_in_test());

    /* Self-Talk test */
    {
        const char *ecdsa_certs[][2] = {
            { S2N_ECDSA_P256_PKCS1_CERT_CHAIN, S2N_ECDSA_P256_PKCS1_KEY },
            { S2N_ECDSA_P384_PKCS1_CERT_CHAIN, S2N_ECDSA_P384_PKCS1_KEY },
            { S2N_ECDSA_P512_CERT_CHAIN, S2N_ECDSA_P512_KEY },
        };

        for (size_t i = 0; i < s2n_array_len(ecdsa_certs); i++) {
            DEFER_CLEANUP(struct s2n_cert_chain_and_key *chain_and_key = NULL,
                    s2n_cert_chain_and_key_ptr_free);
            EXPECT_SUCCESS(s2n_test_cert_chain_and_key_new(&chain_and_key,
                    ecdsa_certs[i][0], ecdsa_certs[i][1]));

            DEFER_CLEANUP(struct s2n_config *config = s2n_config_new(),
                    s2n_config_ptr_free);
            EXPECT_SUCCESS(s2n_config_set_cipher_preferences(config, "test_all"));
            EXPECT_SUCCESS(s2n_config_add_cert_chain_and_key_to_store(config, chain_and_key));
            EXPECT_SUCCESS(s2n_config_set_verification_ca_location(config,
                    ecdsa_certs[i][0], NULL));
            EXPECT_SUCCESS(s2n_config_set_verify_host_callback(config,
                    s2n_test_noop_verify_host_fn, NULL));

            DEFER_CLEANUP(struct s2n_connection *client = s2n_connection_new(S2N_CLIENT),
                    s2n_connection_ptr_free);
            EXPECT_SUCCESS(s2n_connection_set_config(client, config));

            DEFER_CLEANUP(struct s2n_connection *server = s2n_connection_new(S2N_SERVER),
                    s2n_connection_ptr_free);
            EXPECT_SUCCESS(s2n_connection_set_config(server, config));

            DEFER_CLEANUP(struct s2n_test_io_pair io_pair = { 0 },
                    s2n_io_pair_close);
            EXPECT_SUCCESS(s2n_io_pair_init_non_blocking(&io_pair));
            EXPECT_SUCCESS(s2n_connections_set_io_pair(client, server, &io_pair));

            EXPECT_SUCCESS(s2n_negotiate_test_server_and_client(server, client));
        }
    };

    END_TEST();
}
