/**********************************************************************
 * Copyright (c) 2013, 2014, 2015 Pieter Wuille, Gregory Maxwell      *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#if defined HAVE_CONFIG_H
#include "libsecp256k1-config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "secp256k1.c"
#include "include/secp256k1.h"
#include "testrand_impl.h"

#ifdef ENABLE_OPENSSL_TESTS
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/ecdsa.h"
#include "openssl/obj_mac.h"
#endif

#include "contrib/lax_der_parsing.c"
#include "contrib/lax_der_privatekey_parsing.c"

#if !defined(VG_CHECK)
# if defined(VALGRIND)
#  include <valgrind/memcheck.h>
#  define VG_UNDEF(x,y) VALGRIND_MAKE_MEM_UNDEFINED((x),(y))
#  define VG_CHECK(x,y) VALGRIND_CHECK_MEM_IS_DEFINED((x),(y))
# else
#  define VG_UNDEF(x,y)
#  define VG_CHECK(x,y)
# endif
#endif

static int count = 64;
static secp256k1_context *ctx = NULL;

static void counting_illegal_callback_fn(const char* str, void* data) {
    /* Dummy callback function that just counts. */
    int32_t *p;
    (void)str;
    p = data;
    (*p)++;
}

static void uncounting_illegal_callback_fn(const char* str, void* data) {
    /* Dummy callback function that just counts (backwards). */
    int32_t *p;
    (void)str;
    p = data;
    (*p)--;
}

void random_field_element_test(secp256k1_fe *fe) {
    do {
        unsigned char b32[32];
        secp256k1_rand256_test(b32);
        if (secp256k1_fe_set_b32(fe, b32)) {
            break;
        }
    } while(1);
}

void random_field_element_magnitude(secp256k1_fe *fe) {
    secp256k1_fe zero;
    int n = secp256k1_rand_int(9);
    secp256k1_fe_normalize(fe);
    if (n == 0) {
        return;
    }
    secp256k1_fe_clear(&zero);
    secp256k1_fe_negate(&zero, &zero, 0);
    secp256k1_fe_mul_int(&zero, n - 1);
    secp256k1_fe_add(fe, &zero);
    VERIFY_CHECK(fe->magnitude == n);
}

void random_group_element_test(secp256k1_ge *ge) {
    secp256k1_fe fe;
    do {
        random_field_element_test(&fe);
        if (secp256k1_ge_set_xo_var(ge, &fe, secp256k1_rand_bits(1))) {
            secp256k1_fe_normalize(&ge->y);
            break;
        }
    } while(1);
}

void random_group_element_jacobian_test(secp256k1_gej *gej, const secp256k1_ge *ge) {
    secp256k1_fe z2, z3;
    do {
        random_field_element_test(&gej->z);
        if (!secp256k1_fe_is_zero(&gej->z)) {
            break;
        }
    } while(1);
    secp256k1_fe_sqr(&z2, &gej->z);
    secp256k1_fe_mul(&z3, &z2, &gej->z);
    secp256k1_fe_mul(&gej->x, &ge->x, &z2);
    secp256k1_fe_mul(&gej->y, &ge->y, &z3);
    gej->infinity = ge->infinity;
}

void random_scalar_order_test(secp256k1_scalar *num) {
    do {
        unsigned char b32[32];
        int overflow = 0;
        secp256k1_rand256_test(b32);
        secp256k1_scalar_set_b32(num, b32, &overflow);
        if (overflow || secp256k1_scalar_is_zero(num)) {
            continue;
        }
        break;
    } while(1);
}

void random_scalar_order(secp256k1_scalar *num) {
    do {
        unsigned char b32[32];
        int overflow = 0;
        secp256k1_rand256(b32);
        secp256k1_scalar_set_b32(num, b32, &overflow);
        if (overflow || secp256k1_scalar_is_zero(num)) {
            continue;
        }
        break;
    } while(1);
}

void run_context_tests(void) {
    secp256k1_pubkey pubkey;
    secp256k1_pubkey zero_pubkey;
    secp256k1_ecdsa_signature sig;
    unsigned char ctmp[32];
    int32_t ecount;
    int32_t ecount2;
    secp256k1_context *none = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    secp256k1_context *sign = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_context *vrfy = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    secp256k1_context *both = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);

    secp256k1_gej pubj;
    secp256k1_ge pub;
    secp256k1_scalar msg, key, nonce;
    secp256k1_scalar sigr, sigs;

    memset(&zero_pubkey, 0, sizeof(zero_pubkey));

    ecount = 0;
    ecount2 = 10;
    secp256k1_context_set_illegal_callback(vrfy, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(sign, counting_illegal_callback_fn, &ecount2);
    secp256k1_context_set_error_callback(sign, counting_illegal_callback_fn, NULL);
    CHECK(vrfy->error_callback.fn != sign->error_callback.fn);

    /*** clone and destroy all of them to make sure cloning was complete ***/
    {
        secp256k1_context *ctx_tmp;

        ctx_tmp = none; none = secp256k1_context_clone(none); secp256k1_context_destroy(ctx_tmp);
        ctx_tmp = sign; sign = secp256k1_context_clone(sign); secp256k1_context_destroy(ctx_tmp);
        ctx_tmp = vrfy; vrfy = secp256k1_context_clone(vrfy); secp256k1_context_destroy(ctx_tmp);
        ctx_tmp = both; both = secp256k1_context_clone(both); secp256k1_context_destroy(ctx_tmp);
    }

    /* Verify that the error callback makes it across the clone. */
    CHECK(vrfy->error_callback.fn != sign->error_callback.fn);
    /* And that it resets back to default. */
    secp256k1_context_set_error_callback(sign, NULL, NULL);
    CHECK(vrfy->error_callback.fn == sign->error_callback.fn);

    /*** attempt to use them ***/
    random_scalar_order_test(&msg);
    random_scalar_order_test(&key);
    secp256k1_ecmult_gen(&both->ecmult_gen_ctx, &pubj, &key);
    secp256k1_ge_set_gej(&pub, &pubj);

    /* Verify context-type checking illegal-argument errors. */
    memset(ctmp, 1, 32);
    CHECK(secp256k1_ec_pubkey_create(vrfy, &pubkey, ctmp) == 0);
    CHECK(ecount == 1);
    VG_UNDEF(&pubkey, sizeof(pubkey));
    CHECK(secp256k1_ec_pubkey_create(sign, &pubkey, ctmp) == 1);
    VG_CHECK(&pubkey, sizeof(pubkey));
    CHECK(secp256k1_ecdsa_sign(vrfy, &sig, ctmp, ctmp, NULL, NULL) == 0);
    CHECK(ecount == 2);
    VG_UNDEF(&sig, sizeof(sig));
    CHECK(secp256k1_ecdsa_sign(sign, &sig, ctmp, ctmp, NULL, NULL) == 1);
    VG_CHECK(&sig, sizeof(sig));
    CHECK(ecount2 == 10);
    CHECK(secp256k1_ecdsa_verify(sign, &sig, ctmp, &pubkey) == 0);
    CHECK(ecount2 == 11);
    CHECK(secp256k1_ecdsa_verify(vrfy, &sig, ctmp, &pubkey) == 1);
    CHECK(ecount == 2);
    CHECK(secp256k1_ec_pubkey_tweak_add(sign, &pubkey, ctmp) == 0);
    CHECK(ecount2 == 12);
    CHECK(secp256k1_ec_pubkey_tweak_add(vrfy, &pubkey, ctmp) == 1);
    CHECK(ecount == 2);
    CHECK(secp256k1_ec_pubkey_tweak_mul(sign, &pubkey, ctmp) == 0);
    CHECK(ecount2 == 13);
    CHECK(secp256k1_ec_pubkey_negate(vrfy, &pubkey) == 1);
    CHECK(ecount == 2);
    CHECK(secp256k1_ec_pubkey_negate(sign, &pubkey) == 1);
    CHECK(ecount == 2);
    CHECK(secp256k1_ec_pubkey_negate(sign, NULL) == 0);
    CHECK(ecount2 == 14);
    CHECK(secp256k1_ec_pubkey_negate(vrfy, &zero_pubkey) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_ec_pubkey_tweak_mul(vrfy, &pubkey, ctmp) == 1);
    CHECK(ecount == 3);
    CHECK(secp256k1_context_randomize(vrfy, ctmp) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_context_randomize(sign, NULL) == 1);
    CHECK(ecount2 == 14);
    secp256k1_context_set_illegal_callback(vrfy, NULL, NULL);
    secp256k1_context_set_illegal_callback(sign, NULL, NULL);

    /* This shouldn't leak memory, due to already-set tests. */
    secp256k1_ecmult_gen_context_build(&sign->ecmult_gen_ctx, NULL);
    secp256k1_ecmult_context_build(&vrfy->ecmult_ctx, NULL);

    /* obtain a working nonce */
    do {
        random_scalar_order_test(&nonce);
    } while(!secp256k1_ecdsa_sig_sign(&both->ecmult_gen_ctx, &sigr, &sigs, &key, &msg, &nonce, NULL));

    /* try signing */
    CHECK(secp256k1_ecdsa_sig_sign(&sign->ecmult_gen_ctx, &sigr, &sigs, &key, &msg, &nonce, NULL));
    CHECK(secp256k1_ecdsa_sig_sign(&both->ecmult_gen_ctx, &sigr, &sigs, &key, &msg, &nonce, NULL));

    /* try verifying */
    CHECK(secp256k1_ecdsa_sig_verify(&vrfy->ecmult_ctx, &sigr, &sigs, &pub, &msg));
    CHECK(secp256k1_ecdsa_sig_verify(&both->ecmult_ctx, &sigr, &sigs, &pub, &msg));

    /* cleanup */
    secp256k1_context_destroy(none);
    secp256k1_context_destroy(sign);
    secp256k1_context_destroy(vrfy);
    secp256k1_context_destroy(both);
    /* Defined as no-op. */
    secp256k1_context_destroy(NULL);
}

/***** HASH TESTS *****/

void run_sha256_tests(void) {
    static const char *inputs[8] = {
        "", "abc", "message digest", "secure hash algorithm", "SHA256 is considered to be safe",
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
        "For this sample, this 63-byte string will be used as input data",
        "This is exactly 64 bytes long, not counting the terminating byte"
    };
    static const unsigned char outputs[8][32] = {
        {0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55},
        {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad},
        {0xf7, 0x84, 0x6f, 0x55, 0xcf, 0x23, 0xe1, 0x4e, 0xeb, 0xea, 0xb5, 0xb4, 0xe1, 0x55, 0x0c, 0xad, 0x5b, 0x50, 0x9e, 0x33, 0x48, 0xfb, 0xc4, 0xef, 0xa3, 0xa1, 0x41, 0x3d, 0x39, 0x3c, 0xb6, 0x50},
        {0xf3, 0x0c, 0xeb, 0x2b, 0xb2, 0x82, 0x9e, 0x79, 0xe4, 0xca, 0x97, 0x53, 0xd3, 0x5a, 0x8e, 0xcc, 0x00, 0x26, 0x2d, 0x16, 0x4c, 0xc0, 0x77, 0x08, 0x02, 0x95, 0x38, 0x1c, 0xbd, 0x64, 0x3f, 0x0d},
        {0x68, 0x19, 0xd9, 0x15, 0xc7, 0x3f, 0x4d, 0x1e, 0x77, 0xe4, 0xe1, 0xb5, 0x2d, 0x1f, 0xa0, 0xf9, 0xcf, 0x9b, 0xea, 0xea, 0xd3, 0x93, 0x9f, 0x15, 0x87, 0x4b, 0xd9, 0x88, 0xe2, 0xa2, 0x36, 0x30},
        {0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39, 0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67, 0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1},
        {0xf0, 0x8a, 0x78, 0xcb, 0xba, 0xee, 0x08, 0x2b, 0x05, 0x2a, 0xe0, 0x70, 0x8f, 0x32, 0xfa, 0x1e, 0x50, 0xc5, 0xc4, 0x21, 0xaa, 0x77, 0x2b, 0xa5, 0xdb, 0xb4, 0x06, 0xa2, 0xea, 0x6b, 0xe3, 0x42},
        {0xab, 0x64, 0xef, 0xf7, 0xe8, 0x8e, 0x2e, 0x46, 0x16, 0x5e, 0x29, 0xf2, 0xbc, 0xe4, 0x18, 0x26, 0xbd, 0x4c, 0x7b, 0x35, 0x52, 0xf6, 0xb3, 0x82, 0xa9, 0xe7, 0xd3, 0xaf, 0x47, 0xc2, 0x45, 0xf8}
    };
    int i;
    for (i = 0; i < 8; i++) {
        unsigned char out[32];
        secp256k1_sha256 hasher;
        secp256k1_sha256_initialize(&hasher);
        secp256k1_sha256_write(&hasher, (const unsigned char*)(inputs[i]), strlen(inputs[i]));
        secp256k1_sha256_finalize(&hasher, out);
        CHECK(memcmp(out, outputs[i], 32) == 0);
        if (strlen(inputs[i]) > 0) {
            int split = secp256k1_rand_int(strlen(inputs[i]));
            secp256k1_sha256_initialize(&hasher);
            secp256k1_sha256_write(&hasher, (const unsigned char*)(inputs[i]), split);
            secp256k1_sha256_write(&hasher, (const unsigned char*)(inputs[i] + split), strlen(inputs[i]) - split);
            secp256k1_sha256_finalize(&hasher, out);
            CHECK(memcmp(out, outputs[i], 32) == 0);
        }
    }
}

void run_hmac_sha256_tests(void) {
    static const char *keys[6] = {
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
        "\x4a\x65\x66\x65",
        "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
        "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19",
        "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
        "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
    };
    static const char *inputs[6] = {
        "\x48\x69\x20\x54\x68\x65\x72\x65",
        "\x77\x68\x61\x74\x20\x64\x6f\x20\x79\x61\x20\x77\x61\x6e\x74\x20\x66\x6f\x72\x20\x6e\x6f\x74\x68\x69\x6e\x67\x3f",
        "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
        "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
        "\x54\x65\x73\x74\x20\x55\x73\x69\x6e\x67\x20\x4c\x61\x72\x67\x65\x72\x20\x54\x68\x61\x6e\x20\x42\x6c\x6f\x63\x6b\x2d\x53\x69\x7a\x65\x20\x4b\x65\x79\x20\x2d\x20\x48\x61\x73\x68\x20\x4b\x65\x79\x20\x46\x69\x72\x73\x74",
        "\x54\x68\x69\x73\x20\x69\x73\x20\x61\x20\x74\x65\x73\x74\x20\x75\x73\x69\x6e\x67\x20\x61\x20\x6c\x61\x72\x67\x65\x72\x20\x74\x68\x61\x6e\x20\x62\x6c\x6f\x63\x6b\x2d\x73\x69\x7a\x65\x20\x6b\x65\x79\x20\x61\x6e\x64\x20\x61\x20\x6c\x61\x72\x67\x65\x72\x20\x74\x68\x61\x6e\x20\x62\x6c\x6f\x63\x6b\x2d\x73\x69\x7a\x65\x20\x64\x61\x74\x61\x2e\x20\x54\x68\x65\x20\x6b\x65\x79\x20\x6e\x65\x65\x64\x73\x20\x74\x6f\x20\x62\x65\x20\x68\x61\x73\x68\x65\x64\x20\x62\x65\x66\x6f\x72\x65\x20\x62\x65\x69\x6e\x67\x20\x75\x73\x65\x64\x20\x62\x79\x20\x74\x68\x65\x20\x48\x4d\x41\x43\x20\x61\x6c\x67\x6f\x72\x69\x74\x68\x6d\x2e"
    };
    static const unsigned char outputs[6][32] = {
        {0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7},
        {0x5b, 0xdc, 0xc1, 0x46, 0xbf, 0x60, 0x75, 0x4e, 0x6a, 0x04, 0x24, 0x26, 0x08, 0x95, 0x75, 0xc7, 0x5a, 0x00, 0x3f, 0x08, 0x9d, 0x27, 0x39, 0x83, 0x9d, 0xec, 0x58, 0xb9, 0x64, 0xec, 0x38, 0x43},
        {0x77, 0x3e, 0xa9, 0x1e, 0x36, 0x80, 0x0e, 0x46, 0x85, 0x4d, 0xb8, 0xeb, 0xd0, 0x91, 0x81, 0xa7, 0x29, 0x59, 0x09, 0x8b, 0x3e, 0xf8, 0xc1, 0x22, 0xd9, 0x63, 0x55, 0x14, 0xce, 0xd5, 0x65, 0xfe},
        {0x82, 0x55, 0x8a, 0x38, 0x9a, 0x44, 0x3c, 0x0e, 0xa4, 0xcc, 0x81, 0x98, 0x99, 0xf2, 0x08, 0x3a, 0x85, 0xf0, 0xfa, 0xa3, 0xe5, 0x78, 0xf8, 0x07, 0x7a, 0x2e, 0x3f, 0xf4, 0x67, 0x29, 0x66, 0x5b},
        {0x60, 0xe4, 0x31, 0x59, 0x1e, 0xe0, 0xb6, 0x7f, 0x0d, 0x8a, 0x26, 0xaa, 0xcb, 0xf5, 0xb7, 0x7f, 0x8e, 0x0b, 0xc6, 0x21, 0x37, 0x28, 0xc5, 0x14, 0x05, 0x46, 0x04, 0x0f, 0x0e, 0xe3, 0x7f, 0x54},
        {0x9b, 0x09, 0xff, 0xa7, 0x1b, 0x94, 0x2f, 0xcb, 0x27, 0x63, 0x5f, 0xbc, 0xd5, 0xb0, 0xe9, 0x44, 0xbf, 0xdc, 0x63, 0x64, 0x4f, 0x07, 0x13, 0x93, 0x8a, 0x7f, 0x51, 0x53, 0x5c, 0x3a, 0x35, 0xe2}
    };
    int i;
    for (i = 0; i < 6; i++) {
        secp256k1_hmac_sha256 hasher;
        unsigned char out[32];
        secp256k1_hmac_sha256_initialize(&hasher, (const unsigned char*)(keys[i]), strlen(keys[i]));
        secp256k1_hmac_sha256_write(&hasher, (const unsigned char*)(inputs[i]), strlen(inputs[i]));
        secp256k1_hmac_sha256_finalize(&hasher, out);
        CHECK(memcmp(out, outputs[i], 32) == 0);
        if (strlen(inputs[i]) > 0) {
            int split = secp256k1_rand_int(strlen(inputs[i]));
            secp256k1_hmac_sha256_initialize(&hasher, (const unsigned char*)(keys[i]), strlen(keys[i]));
            secp256k1_hmac_sha256_write(&hasher, (const unsigned char*)(inputs[i]), split);
            secp256k1_hmac_sha256_write(&hasher, (const unsigned char*)(inputs[i] + split), strlen(inputs[i]) - split);
            secp256k1_hmac_sha256_finalize(&hasher, out);
            CHECK(memcmp(out, outputs[i], 32) == 0);
        }
    }
}

void run_rfc6979_hmac_sha256_tests(void) {
    static const unsigned char key1[65] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x00, 0x4b, 0xf5, 0x12, 0x2f, 0x34, 0x45, 0x54, 0xc5, 0x3b, 0xde, 0x2e, 0xbb, 0x8c, 0xd2, 0xb7, 0xe3, 0xd1, 0x60, 0x0a, 0xd6, 0x31, 0xc3, 0x85, 0xa5, 0xd7, 0xcc, 0xe2, 0x3c, 0x77, 0x85, 0x45, 0x9a, 0};
    static const unsigned char out1[3][32] = {
        {0x4f, 0xe2, 0x95, 0x25, 0xb2, 0x08, 0x68, 0x09, 0x15, 0x9a, 0xcd, 0xf0, 0x50, 0x6e, 0xfb, 0x86, 0xb0, 0xec, 0x93, 0x2c, 0x7b, 0xa4, 0x42, 0x56, 0xab, 0x32, 0x1e, 0x42, 0x1e, 0x67, 0xe9, 0xfb},
        {0x2b, 0xf0, 0xff, 0xf1, 0xd3, 0xc3, 0x78, 0xa2, 0x2d, 0xc5, 0xde, 0x1d, 0x85, 0x65, 0x22, 0x32, 0x5c, 0x65, 0xb5, 0x04, 0x49, 0x1a, 0x0c, 0xbd, 0x01, 0xcb, 0x8f, 0x3a, 0xa6, 0x7f, 0xfd, 0x4a},
        {0xf5, 0x28, 0xb4, 0x10, 0xcb, 0x54, 0x1f, 0x77, 0x00, 0x0d, 0x7a, 0xfb, 0x6c, 0x5b, 0x53, 0xc5, 0xc4, 0x71, 0xea, 0xb4, 0x3e, 0x46, 0x6d, 0x9a, 0xc5, 0x19, 0x0c, 0x39, 0xc8, 0x2f, 0xd8, 0x2e}
    };

    static const unsigned char key2[64] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};
    static const unsigned char out2[3][32] = {
        {0x9c, 0x23, 0x6c, 0x16, 0x5b, 0x82, 0xae, 0x0c, 0xd5, 0x90, 0x65, 0x9e, 0x10, 0x0b, 0x6b, 0xab, 0x30, 0x36, 0xe7, 0xba, 0x8b, 0x06, 0x74, 0x9b, 0xaf, 0x69, 0x81, 0xe1, 0x6f, 0x1a, 0x2b, 0x95},
        {0xdf, 0x47, 0x10, 0x61, 0x62, 0x5b, 0xc0, 0xea, 0x14, 0xb6, 0x82, 0xfe, 0xee, 0x2c, 0x9c, 0x02, 0xf2, 0x35, 0xda, 0x04, 0x20, 0x4c, 0x1d, 0x62, 0xa1, 0x53, 0x6c, 0x6e, 0x17, 0xae, 0xd7, 0xa9},
        {0x75, 0x97, 0x88, 0x7c, 0xbd, 0x76, 0x32, 0x1f, 0x32, 0xe3, 0x04, 0x40, 0x67, 0x9a, 0x22, 0xcf, 0x7f, 0x8d, 0x9d, 0x2e, 0xac, 0x39, 0x0e, 0x58, 0x1f, 0xea, 0x09, 0x1c, 0xe2, 0x02, 0xba, 0x94}
    };

    secp256k1_rfc6979_hmac_sha256 rng;
    unsigned char out[32];
    int i;

    secp256k1_rfc6979_hmac_sha256_initialize(&rng, key1, 64);
    for (i = 0; i < 3; i++) {
        secp256k1_rfc6979_hmac_sha256_generate(&rng, out, 32);
        CHECK(memcmp(out, out1[i], 32) == 0);
    }
    secp256k1_rfc6979_hmac_sha256_finalize(&rng);

    secp256k1_rfc6979_hmac_sha256_initialize(&rng, key1, 65);
    for (i = 0; i < 3; i++) {
        secp256k1_rfc6979_hmac_sha256_generate(&rng, out, 32);
        CHECK(memcmp(out, out1[i], 32) != 0);
    }
    secp256k1_rfc6979_hmac_sha256_finalize(&rng);

    secp256k1_rfc6979_hmac_sha256_initialize(&rng, key2, 64);
    for (i = 0; i < 3; i++) {
        secp256k1_rfc6979_hmac_sha256_generate(&rng, out, 32);
        CHECK(memcmp(out, out2[i], 32) == 0);
    }
    secp256k1_rfc6979_hmac_sha256_finalize(&rng);
}

/***** RANDOM TESTS *****/

void test_rand_bits(int rand32, int bits) {
    /* (1-1/2^B)^rounds[B] < 1/10^9, so rounds is the number of iterations to
     * get a false negative chance below once in a billion */
    static const unsigned int rounds[7] = {1, 30, 73, 156, 322, 653, 1316};
    /* We try multiplying the results with various odd numbers, which shouldn't
     * influence the uniform distribution modulo a power of 2. */
    static const uint32_t mults[6] = {1, 3, 21, 289, 0x9999, 0x80402011};
    /* We only select up to 6 bits from the output to analyse */
    unsigned int usebits = bits > 6 ? 6 : bits;
    unsigned int maxshift = bits - usebits;
    /* For each of the maxshift+1 usebits-bit sequences inside a bits-bit
       number, track all observed outcomes, one per bit in a uint64_t. */
    uint64_t x[6][27] = {{0}};
    unsigned int i, shift, m;
    /* Multiply the output of all rand calls with the odd number m, which
       should not change the uniformity of its distribution. */
    for (i = 0; i < rounds[usebits]; i++) {
        uint32_t r = (rand32 ? secp256k1_rand32() : secp256k1_rand_bits(bits));
        CHECK((((uint64_t)r) >> bits) == 0);
        for (m = 0; m < sizeof(mults) / sizeof(mults[0]); m++) {
            uint32_t rm = r * mults[m];
            for (shift = 0; shift <= maxshift; shift++) {
                x[m][shift] |= (((uint64_t)1) << ((rm >> shift) & ((1 << usebits) - 1)));
            }
        }
    }
    for (m = 0; m < sizeof(mults) / sizeof(mults[0]); m++) {
        for (shift = 0; shift <= maxshift; shift++) {
            /* Test that the lower usebits bits of x[shift] are 1 */
            CHECK(((~x[m][shift]) << (64 - (1 << usebits))) == 0);
        }
    }
}

/* Subrange must be a whole divisor of range, and at most 64 */
void test_rand_int(uint32_t range, uint32_t subrange) {
    /* (1-1/subrange)^rounds < 1/10^9 */
    int rounds = (subrange * 2073) / 100;
    int i;
    uint64_t x = 0;
    CHECK((range % subrange) == 0);
    for (i = 0; i < rounds; i++) {
        uint32_t r = secp256k1_rand_int(range);
        CHECK(r < range);
        r = r % subrange;
        x |= (((uint64_t)1) << r);
    }
    /* Test that the lower subrange bits of x are 1. */
    CHECK(((~x) << (64 - subrange)) == 0);
}

void run_rand_bits(void) {
    size_t b;
    test_rand_bits(1, 32);
    for (b = 1; b <= 32; b++) {
        test_rand_bits(0, b);
    }
}

void run_rand_int(void) {
    static const uint32_t ms[] = {1, 3, 17, 1000, 13771, 999999, 33554432};
    static const uint32_t ss[] = {1, 3, 6, 9, 13, 31, 64};
    unsigned int m, s;
    for (m = 0; m < sizeof(ms) / sizeof(ms[0]); m++) {
        for (s = 0; s < sizeof(ss) / sizeof(ss[0]); s++) {
            test_rand_int(ms[m] * ss[s], ss[s]);
        }
    }
}

/***** NUM TESTS *****/

#ifndef USE_NUM_NONE
void random_num_negate(secp256k1_num *num) {
    if (secp256k1_rand_bits(1)) {
        secp256k1_num_negate(num);
    }
}

void random_num_order_test(secp256k1_num *num) {
    secp256k1_scalar sc;
    random_scalar_order_test(&sc);
    secp256k1_scalar_get_num(num, &sc);
}

void random_num_order(secp256k1_num *num) {
    secp256k1_scalar sc;
    random_scalar_order(&sc);
    secp256k1_scalar_get_num(num, &sc);
}

void test_num_negate(void) {
    secp256k1_num n1;
    secp256k1_num n2;
    random_num_order_test(&n1); /* n1 = R */
    random_num_negate(&n1);
    secp256k1_num_copy(&n2, &n1); /* n2 = R */
    secp256k1_num_sub(&n1, &n2, &n1); /* n1 = n2-n1 = 0 */
    CHECK(secp256k1_num_is_zero(&n1));
    secp256k1_num_copy(&n1, &n2); /* n1 = R */
    secp256k1_num_negate(&n1); /* n1 = -R */
    CHECK(!secp256k1_num_is_zero(&n1));
    secp256k1_num_add(&n1, &n2, &n1); /* n1 = n2+n1 = 0 */
    CHECK(secp256k1_num_is_zero(&n1));
    secp256k1_num_copy(&n1, &n2); /* n1 = R */
    secp256k1_num_negate(&n1); /* n1 = -R */
    CHECK(secp256k1_num_is_neg(&n1) != secp256k1_num_is_neg(&n2));
    secp256k1_num_negate(&n1); /* n1 = R */
    CHECK(secp256k1_num_eq(&n1, &n2));
}

void test_num_add_sub(void) {
    int i;
    secp256k1_scalar s;
    secp256k1_num n1;
    secp256k1_num n2;
    secp256k1_num n1p2, n2p1, n1m2, n2m1;
    random_num_order_test(&n1); /* n1 = R1 */
    if (secp256k1_rand_bits(1)) {
        random_num_negate(&n1);
    }
    random_num_order_test(&n2); /* n2 = R2 */
    if (secp256k1_rand_bits(1)) {
        random_num_negate(&n2);
    }
    secp256k1_num_add(&n1p2, &n1, &n2); /* n1p2 = R1 + R2 */
    secp256k1_num_add(&n2p1, &n2, &n1); /* n2p1 = R2 + R1 */
    secp256k1_num_sub(&n1m2, &n1, &n2); /* n1m2 = R1 - R2 */
    secp256k1_num_sub(&n2m1, &n2, &n1); /* n2m1 = R2 - R1 */
    CHECK(secp256k1_num_eq(&n1p2, &n2p1));
    CHECK(!secp256k1_num_eq(&n1p2, &n1m2));
    secp256k1_num_negate(&n2m1); /* n2m1 = -R2 + R1 */
    CHECK(secp256k1_num_eq(&n2m1, &n1m2));
    CHECK(!secp256k1_num_eq(&n2m1, &n1));
    secp256k1_num_add(&n2m1, &n2m1, &n2); /* n2m1 = -R2 + R1 + R2 = R1 */
    CHECK(secp256k1_num_eq(&n2m1, &n1));
    CHECK(!secp256k1_num_eq(&n2p1, &n1));
    secp256k1_num_sub(&n2p1, &n2p1, &n2); /* n2p1 = R2 + R1 - R2 = R1 */
    CHECK(secp256k1_num_eq(&n2p1, &n1));

    /* check is_one */
    secp256k1_scalar_set_int(&s, 1);
    secp256k1_scalar_get_num(&n1, &s);
    CHECK(secp256k1_num_is_one(&n1));
    /* check that 2^n + 1 is never 1 */
    secp256k1_scalar_get_num(&n2, &s);
    for (i = 0; i < 250; ++i) {
        secp256k1_num_add(&n1, &n1, &n1);    /* n1 *= 2 */
        secp256k1_num_add(&n1p2, &n1, &n2);  /* n1p2 = n1 + 1 */
        CHECK(!secp256k1_num_is_one(&n1p2));
    }
}

void test_num_mod(void) {
    int i;
    secp256k1_scalar s;
    secp256k1_num order, n;

    /* check that 0 mod anything is 0 */
    random_scalar_order_test(&s);
    secp256k1_scalar_get_num(&order, &s);
    secp256k1_scalar_set_int(&s, 0);
    secp256k1_scalar_get_num(&n, &s);
    secp256k1_num_mod(&n, &order);
    CHECK(secp256k1_num_is_zero(&n));

    /* check that anything mod 1 is 0 */
    secp256k1_scalar_set_int(&s, 1);
    secp256k1_scalar_get_num(&order, &s);
    secp256k1_scalar_get_num(&n, &s);
    secp256k1_num_mod(&n, &order);
    CHECK(secp256k1_num_is_zero(&n));

    /* check that increasing the number past 2^256 does not break this */
    random_scalar_order_test(&s);
    secp256k1_scalar_get_num(&n, &s);
    /* multiply by 2^8, which'll test this case with high probability */
    for (i = 0; i < 8; ++i) {
        secp256k1_num_add(&n, &n, &n);
    }
    secp256k1_num_mod(&n, &order);
    CHECK(secp256k1_num_is_zero(&n));
}

void test_num_jacobi(void) {
    secp256k1_scalar sqr;
    secp256k1_scalar small;
    secp256k1_scalar five;  /* five is not a quadratic residue */
    secp256k1_num order, n;
    int i;
    /* squares mod 5 are 1, 4 */
    const int jacobi5[10] = { 0, 1, -1, -1, 1, 0, 1, -1, -1, 1 };

    /* check some small values with 5 as the order */
    secp256k1_scalar_set_int(&five, 5);
    secp256k1_scalar_get_num(&order, &five);
    for (i = 0; i < 10; ++i) {
        secp256k1_scalar_set_int(&small, i);
        secp256k1_scalar_get_num(&n, &small);
        CHECK(secp256k1_num_jacobi(&n, &order) == jacobi5[i]);
    }

    /** test large values with 5 as group order */
    secp256k1_scalar_get_num(&order, &five);
    /* we first need a scalar which is not a multiple of 5 */
    do {
        secp256k1_num fiven;
        random_scalar_order_test(&sqr);
        secp256k1_scalar_get_num(&fiven, &five);
        secp256k1_scalar_get_num(&n, &sqr);
        secp256k1_num_mod(&n, &fiven);
    } while (secp256k1_num_is_zero(&n));
    /* next force it to be a residue. 2 is a nonresidue mod 5 so we can
     * just multiply by two, i.e. add the number to itself */
    if (secp256k1_num_jacobi(&n, &order) == -1) {
        secp256k1_num_add(&n, &n, &n);
    }

    /* test residue */
    CHECK(secp256k1_num_jacobi(&n, &order) == 1);
    /* test nonresidue */
    secp256k1_num_add(&n, &n, &n);
    CHECK(secp256k1_num_jacobi(&n, &order) == -1);

    /** test with secp group order as order */
    secp256k1_scalar_order_get_num(&order);
    random_scalar_order_test(&sqr);
    secp256k1_scalar_sqr(&sqr, &sqr);
    /* test residue */
    secp256k1_scalar_get_num(&n, &sqr);
    CHECK(secp256k1_num_jacobi(&n, &order) == 1);
    /* test nonresidue */
    secp256k1_scalar_mul(&sqr, &sqr, &five);
    secp256k1_scalar_get_num(&n, &sqr);
    CHECK(secp256k1_num_jacobi(&n, &order) == -1);
    /* test multiple of the order*/
    CHECK(secp256k1_num_jacobi(&order, &order) == 0);

    /* check one less than the order */
    secp256k1_scalar_set_int(&small, 1);
    secp256k1_scalar_get_num(&n, &small);
    secp256k1_num_sub(&n, &order, &n);
    CHECK(secp256k1_num_jacobi(&n, &order) == 1);  /* sage confirms this is 1 */
}

void run_num_smalltests(void) {
    int i;
    for (i = 0; i < 100*count; i++) {
        test_num_negate();
        test_num_add_sub();
        test_num_mod();
        test_num_jacobi();
    }
}
#endif

/***** SCALAR TESTS *****/

void scalar_test(void) {
    secp256k1_scalar s;
    secp256k1_scalar s1;
    secp256k1_scalar s2;
#ifndef USE_NUM_NONE
    secp256k1_num snum, s1num, s2num;
    secp256k1_num order, half_order;
#endif
    unsigned char c[32];

    /* Set 's' to a random scalar, with value 'snum'. */
    random_scalar_order_test(&s);

    /* Set 's1' to a random scalar, with value 's1num'. */
    random_scalar_order_test(&s1);

    /* Set 's2' to a random scalar, with value 'snum2', and byte array representation 'c'. */
    random_scalar_order_test(&s2);
    secp256k1_scalar_get_b32(c, &s2);

#ifndef USE_NUM_NONE
    secp256k1_scalar_get_num(&snum, &s);
    secp256k1_scalar_get_num(&s1num, &s1);
    secp256k1_scalar_get_num(&s2num, &s2);

    secp256k1_scalar_order_get_num(&order);
    half_order = order;
    secp256k1_num_shift(&half_order, 1);
#endif

    {
        int i;
        /* Test that fetching groups of 4 bits from a scalar and recursing n(i)=16*n(i-1)+p(i) reconstructs it. */
        secp256k1_scalar n;
        secp256k1_scalar_set_int(&n, 0);
        for (i = 0; i < 256; i += 4) {
            secp256k1_scalar t;
            int j;
            secp256k1_scalar_set_int(&t, secp256k1_scalar_get_bits(&s, 256 - 4 - i, 4));
            for (j = 0; j < 4; j++) {
                secp256k1_scalar_add(&n, &n, &n);
            }
            secp256k1_scalar_add(&n, &n, &t);
        }
        CHECK(secp256k1_scalar_eq(&n, &s));
    }

    {
        /* Test that fetching groups of randomly-sized bits from a scalar and recursing n(i)=b*n(i-1)+p(i) reconstructs it. */
        secp256k1_scalar n;
        int i = 0;
        secp256k1_scalar_set_int(&n, 0);
        while (i < 256) {
            secp256k1_scalar t;
            int j;
            int now = secp256k1_rand_int(15) + 1;
            if (now + i > 256) {
                now = 256 - i;
            }
            secp256k1_scalar_set_int(&t, secp256k1_scalar_get_bits_var(&s, 256 - now - i, now));
            for (j = 0; j < now; j++) {
                secp256k1_scalar_add(&n, &n, &n);
            }
            secp256k1_scalar_add(&n, &n, &t);
            i += now;
        }
        CHECK(secp256k1_scalar_eq(&n, &s));
    }

#ifndef USE_NUM_NONE
    {
        /* Test that adding the scalars together is equal to adding their numbers together modulo the order. */
        secp256k1_num rnum;
        secp256k1_num r2num;
        secp256k1_scalar r;
        secp256k1_num_add(&rnum, &snum, &s2num);
        secp256k1_num_mod(&rnum, &order);
        secp256k1_scalar_add(&r, &s, &s2);
        secp256k1_scalar_get_num(&r2num, &r);
        CHECK(secp256k1_num_eq(&rnum, &r2num));
    }

    {
        /* Test that multiplying the scalars is equal to multiplying their numbers modulo the order. */
        secp256k1_scalar r;
        secp256k1_num r2num;
        secp256k1_num rnum;
        secp256k1_num_mul(&rnum, &snum, &s2num);
        secp256k1_num_mod(&rnum, &order);
        secp256k1_scalar_mul(&r, &s, &s2);
        secp256k1_scalar_get_num(&r2num, &r);
        CHECK(secp256k1_num_eq(&rnum, &r2num));
        /* The result can only be zero if at least one of the factors was zero. */
        CHECK(secp256k1_scalar_is_zero(&r) == (secp256k1_scalar_is_zero(&s) || secp256k1_scalar_is_zero(&s2)));
        /* The results can only be equal to one of the factors if that factor was zero, or the other factor was one. */
        CHECK(secp256k1_num_eq(&rnum, &snum) == (secp256k1_scalar_is_zero(&s) || secp256k1_scalar_is_one(&s2)));
        CHECK(secp256k1_num_eq(&rnum, &s2num) == (secp256k1_scalar_is_zero(&s2) || secp256k1_scalar_is_one(&s)));
    }

    {
        secp256k1_scalar neg;
        secp256k1_num negnum;
        secp256k1_num negnum2;
        /* Check that comparison with zero matches comparison with zero on the number. */
        CHECK(secp256k1_num_is_zero(&snum) == secp256k1_scalar_is_zero(&s));
        /* Check that comparison with the half order is equal to testing for high scalar. */
        CHECK(secp256k1_scalar_is_high(&s) == (secp256k1_num_cmp(&snum, &half_order) > 0));
        secp256k1_scalar_negate(&neg, &s);
        secp256k1_num_sub(&negnum, &order, &snum);
        secp256k1_num_mod(&negnum, &order);
        /* Check that comparison with the half order is equal to testing for high scalar after negation. */
        CHECK(secp256k1_scalar_is_high(&neg) == (secp256k1_num_cmp(&negnum, &half_order) > 0));
        /* Negating should change the high property, unless the value was already zero. */
        CHECK((secp256k1_scalar_is_high(&s) == secp256k1_scalar_is_high(&neg)) == secp256k1_scalar_is_zero(&s));
        secp256k1_scalar_get_num(&negnum2, &neg);
        /* Negating a scalar should be equal to (order - n) mod order on the number. */
        CHECK(secp256k1