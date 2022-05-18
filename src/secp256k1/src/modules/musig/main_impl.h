
/**********************************************************************
 * Copyright (c) 2018 Andrew Poelstra, Jonas Nick                     *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _SECP256K1_MODULE_MUSIG_MAIN_
#define _SECP256K1_MODULE_MUSIG_MAIN_

#include "../../../include/secp256k1.h"
#include "../../../include/secp256k1_musig.h"
#include "hash.h"

/* Computes ell = SHA256(pk[0], ..., pk[np-1]) */
static int secp256k1_musig_compute_ell(const secp256k1_context *ctx, unsigned char *ell, const secp256k1_pubkey *pk, size_t np) {
    secp256k1_sha256 sha;
    size_t i;

    secp256k1_sha256_initialize(&sha);
    for (i = 0; i < np; i++) {
        unsigned char ser[33];
        size_t serlen = sizeof(ser);
        if (!secp256k1_ec_pubkey_serialize(ctx, ser, &serlen, &pk[i], SECP256K1_EC_COMPRESSED)) {
            return 0;
        }
        secp256k1_sha256_write(&sha, ser, serlen);
    }
    secp256k1_sha256_finalize(&sha, ell);
    return 1;
}

/* Initializes SHA256 with fixed midstate. This midstate was computed by applying
 * SHA256 to SHA256("MuSig coefficient")||SHA256("MuSig coefficient"). */
static void secp256k1_musig_sha256_init_tagged(secp256k1_sha256 *sha) {
    secp256k1_sha256_initialize(sha);

    sha->s[0] = 0x0fd0690cul;
    sha->s[1] = 0xfefeae97ul;
    sha->s[2] = 0x996eac7ful;
    sha->s[3] = 0x5c30d864ul;
    sha->s[4] = 0x8c4a0573ul;
    sha->s[5] = 0xaca1a22ful;
    sha->s[6] = 0x6f43b801ul;
    sha->s[7] = 0x85ce27cdul;
    sha->bytes = 64;
}

/* Compute r = SHA256(ell, idx). The four bytes of idx are serialized least significant byte first. */
static void secp256k1_musig_coefficient(secp256k1_scalar *r, const unsigned char *ell, uint32_t idx) {
    secp256k1_sha256 sha;
    unsigned char buf[32];
    size_t i;

    secp256k1_musig_sha256_init_tagged(&sha);
    secp256k1_sha256_write(&sha, ell, 32);
    /* We're hashing the index of the signer instead of its public key as specified
     * in the MuSig paper. This reduces the total amount of data that needs to be
     * hashed.
     * Additionally, it prevents creating identical musig_coefficients for identical
     * public keys. A participant Bob could choose his public key to be the same as
     * Alice's, then replay Alice's messages (nonce and partial signature) to create
     * a valid partial signature. This is not a problem for MuSig per se, but could
     * result in subtle issues with protocols building on threshold signatures.
     * With the assumption that public keys are unique, hashing the index is
     * equivalent to hashing the public key. Because the public key can be
     * identified by the index given the ordered list of public keys (included in
     * ell), the index is just a different encoding of the public key.*/
    for (i = 0; i < sizeof(uint32_t); i++) {
        unsigned char c = idx;
        secp256k1_sha256_write(&sha, &c, 1);
        idx >>= 8;
    }
    secp256k1_sha256_finalize(&sha, buf);
    secp256k1_scalar_set_b32(r, buf, NULL);
}

typedef struct {
    const secp256k1_context *ctx;
    unsigned char ell[32];
    const secp256k1_pubkey *pks;
} secp256k1_musig_pubkey_combine_ecmult_data;

/* Callback for batch EC multiplication to compute ell_0*P0 + ell_1*P1 + ...  */
static int secp256k1_musig_pubkey_combine_callback(secp256k1_scalar *sc, secp256k1_ge *pt, size_t idx, void *data) {
    secp256k1_musig_pubkey_combine_ecmult_data *ctx = (secp256k1_musig_pubkey_combine_ecmult_data *) data;
    secp256k1_musig_coefficient(sc, ctx->ell, idx);
    return secp256k1_pubkey_load(ctx->ctx, pt, &ctx->pks[idx]);
}


static void secp256k1_musig_signers_init(secp256k1_musig_session_signer_data *signers, uint32_t n_signers) {
    uint32_t i;
    for (i = 0; i < n_signers; i++) {
        memset(&signers[i], 0, sizeof(signers[i]));
        signers[i].index = i;
        signers[i].present = 0;
    }
}

int secp256k1_musig_pubkey_combine(const secp256k1_context* ctx, secp256k1_scratch_space *scratch, secp256k1_pubkey *combined_pk, unsigned char *pk_hash32, const secp256k1_pubkey *pubkeys, size_t n_pubkeys) {
    secp256k1_musig_pubkey_combine_ecmult_data ecmult_data;
    secp256k1_gej pkj;
    secp256k1_ge pkp;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(combined_pk != NULL);
    ARG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    ARG_CHECK(pubkeys != NULL);
    ARG_CHECK(n_pubkeys > 0);

    ecmult_data.ctx = ctx;
    ecmult_data.pks = pubkeys;
    if (!secp256k1_musig_compute_ell(ctx, ecmult_data.ell, pubkeys, n_pubkeys)) {
        return 0;
    }
    if (!secp256k1_ecmult_multi_var(&ctx->ecmult_ctx, scratch, &pkj, NULL, secp256k1_musig_pubkey_combine_callback, (void *) &ecmult_data, n_pubkeys)) {
        return 0;
    }
    secp256k1_ge_set_gej(&pkp, &pkj);
    secp256k1_pubkey_save(combined_pk, &pkp);

    if (pk_hash32 != NULL) {
        memcpy(pk_hash32, ecmult_data.ell, 32);
    }
    return 1;
}

int secp256k1_musig_session_initialize(const secp256k1_context* ctx, secp256k1_musig_session *session, secp256k1_musig_session_signer_data *signers, unsigned char *nonce_commitment32, const unsigned char *session_id32, const unsigned char *msg32, const secp256k1_pubkey *combined_pk, const unsigned char *pk_hash32, size_t n_signers, size_t my_index, const unsigned char *seckey) {
    unsigned char combined_ser[33];
    size_t combined_ser_size = sizeof(combined_ser);
    int overflow;
    secp256k1_scalar secret;
    secp256k1_scalar mu;
    secp256k1_sha256 sha;
    secp256k1_gej rj;
    secp256k1_ge rp;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));
    ARG_CHECK(session != NULL);
    ARG_CHECK(signers != NULL);
    ARG_CHECK(nonce_commitment32 != NULL);
    ARG_CHECK(session_id32 != NULL);
    ARG_CHECK(combined_pk != NULL);
    ARG_CHECK(pk_hash32 != NULL);
    ARG_CHECK(seckey != NULL);

    memset(session, 0, sizeof(*session));

    if (msg32 != NULL) {
        memcpy(session->msg, msg32, 32);
        session->msg_is_set = 1;
    } else {
        session->msg_is_set = 0;
    }
    memcpy(&session->combined_pk, combined_pk, sizeof(*combined_pk));
    memcpy(session->pk_hash, pk_hash32, 32);
    session->nonce_is_set = 0;
    session->has_secret_data = 1;
    if (n_signers == 0 || my_index >= n_signers) {
        return 0;
    }
    if (n_signers > UINT32_MAX) {
        return 0;
    }
    session->n_signers = (uint32_t) n_signers;
    secp256k1_musig_signers_init(signers, session->n_signers);
    session->nonce_commitments_hash_is_set = 0;

    /* Compute secret key */
    secp256k1_scalar_set_b32(&secret, seckey, &overflow);
    if (overflow) {
        secp256k1_scalar_clear(&secret);
        return 0;
    }
    secp256k1_musig_coefficient(&mu, pk_hash32, (uint32_t) my_index);
    secp256k1_scalar_mul(&secret, &secret, &mu);
    secp256k1_scalar_get_b32(session->seckey, &secret);

    /* Compute secret nonce */
    secp256k1_sha256_initialize(&sha);
    secp256k1_sha256_write(&sha, session_id32, 32);
    if (session->msg_is_set) {
        secp256k1_sha256_write(&sha, msg32, 32);
    }
    secp256k1_ec_pubkey_serialize(ctx, combined_ser, &combined_ser_size, combined_pk, SECP256K1_EC_COMPRESSED);
    secp256k1_sha256_write(&sha, combined_ser, combined_ser_size);
    secp256k1_sha256_write(&sha, seckey, 32);
    secp256k1_sha256_finalize(&sha, session->secnonce);
    secp256k1_scalar_set_b32(&secret, session->secnonce, &overflow);
    if (overflow) {
        secp256k1_scalar_clear(&secret);
        return 0;
    }

    /* Compute public nonce and commitment */
    secp256k1_ecmult_gen(&ctx->ecmult_gen_ctx, &rj, &secret);
    secp256k1_ge_set_gej(&rp, &rj);
    secp256k1_pubkey_save(&session->nonce, &rp);

    if (nonce_commitment32 != NULL) {
        unsigned char commit[33];
        size_t commit_size = sizeof(commit);
        secp256k1_sha256_initialize(&sha);
        secp256k1_ec_pubkey_serialize(ctx, commit, &commit_size, &session->nonce, SECP256K1_EC_COMPRESSED);
        secp256k1_sha256_write(&sha, commit, commit_size);
        secp256k1_sha256_finalize(&sha, nonce_commitment32);
    }

    secp256k1_scalar_clear(&secret);
    return 1;
}

int secp256k1_musig_session_get_public_nonce(const secp256k1_context* ctx, secp256