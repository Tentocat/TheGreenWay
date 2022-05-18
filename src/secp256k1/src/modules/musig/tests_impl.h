/**********************************************************************
 * Copyright (c) 2018 Andrew Poelstra                                 *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _SECP256K1_MODULE_MUSIG_TESTS_
#define _SECP256K1_MODULE_MUSIG_TESTS_

#include "secp256k1_musig.h"

void musig_api_tests(secp256k1_scratch_space *scratch) {
    secp256k1_scratch_space *scratch_small;
    secp256k1_musig_session session[2];
    secp256k1_musig_session verifier_session;
    secp256k1_musig_session_signer_data signer0[2];
    secp256k1_musig_session_signer_data signer1[2];
    secp256k1_musig_session_signer_data verifier_signer_data[2];
    secp256k1_musig_partial_signature partial_sig[2];
    secp256k1_musig_partial_signature partial_sig_adapted[2];
    secp256k1_musig_partial_signature partial_sig_overflow;
    secp256k1_schnorrsig final_sig;
    secp256k1_schnorrsig final_sig_cmp;

    unsigned char buf[32];
    unsigned char sk[2][32];
    unsigned char ones[32];
    unsigned char session_id[2][32];
    unsigned char nonce_commitment[2][32];
    int nonce_is_negated;
    const unsigned char *ncs[2];
    unsigned char msg[32];
    unsigned char msghash[32];
    secp256k1_pubkey combined_pk;
    unsigned char pk_hash[32];
    secp256k1_pubkey pk[2];

    unsigned char sec_adaptor[32];
    unsigned char sec_adaptor1[32];
    secp256k1_pubkey adaptor;

    /** setup **/
    secp256k1_context *none = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    secp256k1_context *sign = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_context *vrfy = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    int ecount;

    secp256k1_context_set_error_callback(none, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_error_callback(sign, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_error_callback(vrfy, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(none, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(sign, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(vrfy, counting_illegal_callback_fn, &ecount);

    memset(ones, 0xff, 32);

    secp256k1_rand256(session_id[0]);
    secp256k1_rand256(session_id[1]);
    secp256k1_rand256(sk[0]);
    secp256k1_rand256(sk[1]);
    secp256k1_rand256(msg);
    secp256k1_rand256(sec_adaptor);

    CHECK(secp256k1_ec_pubkey_create(ctx, &pk[0], sk[0]) == 1);
    CHECK(secp256k1_ec_pubkey_create(ctx, &pk[1], sk[1]) == 1);
    CHECK(secp256k1_ec_pubkey_create(ctx, &adaptor, sec_adaptor) == 1);

    /** main test body **/

    /* Key combination */
    ecount = 0;
    CHECK(secp256k1_musig_pubkey_combine(none, scratch, &combined_pk, pk_hash, pk, 2) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_pubkey_combine(sign, scratch, &combined_pk, pk_hash, pk, 2) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, pk, 2) == 1);
    CHECK(ecount == 2);
    /* pubkey_combine does not require a scratch space */
    CHECK(secp256k1_musig_pubkey_combine(vrfy, NULL, &combined_pk, pk_hash, pk, 2) == 1);
    CHECK(ecount == 2);
    /* If a scratch space is given it shouldn't be too small */
    scratch_small = secp256k1_scratch_space_create(ctx, 1);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch_small, &combined_pk, pk_hash, pk, 2) == 0);
    secp256k1_scratch_space_destroy(scratch_small);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, NULL, pk_hash, pk, 2) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, NULL, pk, 2) == 1);
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, NULL, 2) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, pk, 0) == 0);
    CHECK(ecount == 5);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, NULL, 0) == 0);
    CHECK(ecount == 6);

    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, pk, 2) == 1);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, pk, 2) == 1);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, pk, 2) == 1);

    /** Session creation **/
    ecount = 0;
    CHECK(secp256k1_musig_session_initialize(none, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_session_initialize(vrfy, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 1);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_session_initialize(sign, NULL, signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], NULL, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, NULL, session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 5);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], NULL, msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 6);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], NULL, &combined_pk, pk_hash, 2, 0, sk[0]) == 1);
    CHECK(ecount == 6);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, NULL, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 7);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, NULL, 2, 0, sk[0]) == 0);
    CHECK(ecount == 8);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 0, 0, sk[0]) == 0);
    CHECK(ecount == 8);
    /* If more than UINT32_MAX fits in a size_t, test that session_initialize
     * rejects n_signers that high. */
    if (SIZE_MAX > UINT32_MAX) {
        CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, ((size_t) UINT32_MAX) + 2, 0, sk[0]) == 0);
    }
    CHECK(ecount == 8);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, NULL) == 0);
    CHECK(ecount == 9);
    /* secret key overflows */
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, ones) == 0);
    CHECK(ecount == 9);


    {
        secp256k1_musig_session session_without_msg;
        CHECK(secp256k1_musig_session_initialize(sign, &session_without_msg, signer0, nonce_commitment[0], session_id[0], NULL, &combined_pk, pk_hash, 2, 0, sk[0]) == 1);
        CHECK(secp256k1_musig_session_set_msg(none, &session_without_msg, msg) == 1);
        CHECK(secp256k1_musig_session_set_msg(none, &session_without_msg, msg) == 0);
    }
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 1);
    CHECK(secp256k1_musig_session_initialize(sign, &session[1], signer1, nonce_commitment[1], session_id[1], msg, &combined_pk, pk_hash, 2, 1, sk[1]) == 1);
    ncs[0] = nonce_commitment[0];
    ncs[1] = nonce_commitment[1];

    ecount = 0;
    CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, msg, &combined_pk, pk_hash, ncs, 2) == 1);
    CHECK(ecount == 0);
    CHECK(secp256k1_musig_session_initialize_verifier(none, NULL, verifier_signer_data, msg, &combined_pk, pk_hash, ncs, 2) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, NULL, &combined_pk, pk_hash, ncs, 2) == 1);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, msg, NULL, pk_hash, ncs, 2) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_session_initialize_veri