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
        CHECK(secp256k1_num_eq(&negnum, &negnum2));
        secp256k1_scalar_add(&neg, &neg, &s);
        /* Adding a number to its negation should result in zero. */
        CHECK(secp256k1_scalar_is_zero(&neg));
        secp256k1_scalar_negate(&neg, &neg);
        /* Negating zero should still result in zero. */
        CHECK(secp256k1_scalar_is_zero(&neg));
    }

    {
        /* Test secp256k1_scalar_mul_shift_var. */
        secp256k1_scalar r;
        secp256k1_num one;
        secp256k1_num rnum;
        secp256k1_num rnum2;
        unsigned char cone[1] = {0x01};
        unsigned int shift = 256 + secp256k1_rand_int(257);
        secp256k1_scalar_mul_shift_var(&r, &s1, &s2, shift);
        secp256k1_num_mul(&rnum, &s1num, &s2num);
        secp256k1_num_shift(&rnum, shift - 1);
        secp256k1_num_set_bin(&one, cone, 1);
        secp256k1_num_add(&rnum, &rnum, &one);
        secp256k1_num_shift(&rnum, 1);
        secp256k1_scalar_get_num(&rnum2, &r);
        CHECK(secp256k1_num_eq(&rnum, &rnum2));
    }

    {
        /* test secp256k1_scalar_shr_int */
        secp256k1_scalar r;
        int i;
        random_scalar_order_test(&r);
        for (i = 0; i < 100; ++i) {
            int low;
            int shift = 1 + secp256k1_rand_int(15);
            int expected = r.d[0] % (1 << shift);
            low = secp256k1_scalar_shr_int(&r, shift);
            CHECK(expected == low);
        }
    }
#endif

    {
        /* Test that scalar inverses are equal to the inverse of their number modulo the order. */
        if (!secp256k1_scalar_is_zero(&s)) {
            secp256k1_scalar inv;
#ifndef USE_NUM_NONE
            secp256k1_num invnum;
            secp256k1_num invnum2;
#endif
            secp256k1_scalar_inverse(&inv, &s);
#ifndef USE_NUM_NONE
            secp256k1_num_mod_inverse(&invnum, &snum, &order);
            secp256k1_scalar_get_num(&invnum2, &inv);
            CHECK(secp256k1_num_eq(&invnum, &invnum2));
#endif
            secp256k1_scalar_mul(&inv, &inv, &s);
            /* Multiplying a scalar with its inverse must result in one. */
            CHECK(secp256k1_scalar_is_one(&inv));
            secp256k1_scalar_inverse(&inv, &inv);
            /* Inverting one must result in one. */
            CHECK(secp256k1_scalar_is_one(&inv));
#ifndef USE_NUM_NONE
            secp256k1_scalar_get_num(&invnum, &inv);
            CHECK(secp256k1_num_is_one(&invnum));
#endif
        }
    }

    {
        /* Test commutativity of add. */
        secp256k1_scalar r1, r2;
        secp256k1_scalar_add(&r1, &s1, &s2);
        secp256k1_scalar_add(&r2, &s2, &s1);
        CHECK(secp256k1_scalar_eq(&r1, &r2));
    }

    {
        secp256k1_scalar r1, r2;
        secp256k1_scalar b;
        int i;
        /* Test add_bit. */
        int bit = secp256k1_rand_bits(8);
        secp256k1_scalar_set_int(&b, 1);
        CHECK(secp256k1_scalar_is_one(&b));
        for (i = 0; i < bit; i++) {
            secp256k1_scalar_add(&b, &b, &b);
        }
        r1 = s1;
        r2 = s1;
        if (!secp256k1_scalar_add(&r1, &r1, &b)) {
            /* No overflow happened. */
            secp256k1_scalar_cadd_bit(&r2, bit, 1);
            CHECK(secp256k1_scalar_eq(&r1, &r2));
            /* cadd is a noop when flag is zero */
            secp256k1_scalar_cadd_bit(&r2, bit, 0);
            CHECK(secp256k1_scalar_eq(&r1, &r2));
        }
    }

    {
        /* Test commutativity of mul. */
        secp256k1_scalar r1, r2;
        secp256k1_scalar_mul(&r1, &s1, &s2);
        secp256k1_scalar_mul(&r2, &s2, &s1);
        CHECK(secp256k1_scalar_eq(&r1, &r2));
    }

    {
        /* Test associativity of add. */
        secp256k1_scalar r1, r2;
        secp256k1_scalar_add(&r1, &s1, &s2);
        secp256k1_scalar_add(&r1, &r1, &s);
        secp256k1_scalar_add(&r2, &s2, &s);
        secp256k1_scalar_add(&r2, &s1, &r2);
        CHECK(secp256k1_scalar_eq(&r1, &r2));
    }

    {
        /* Test associativity of mul. */
        secp256k1_scalar r1, r2;
        secp256k1_scalar_mul(&r1, &s1, &s2);
        secp256k1_scalar_mul(&r1, &r1, &s);
        secp256k1_scalar_mul(&r2, &s2, &s);
        secp256k1_scalar_mul(&r2, &s1, &r2);
        CHECK(secp256k1_scalar_eq(&r1, &r2));
    }

    {
        /* Test distributitivity of mul over add. */
        secp256k1_scalar r1, r2, t;
        secp256k1_scalar_add(&r1, &s1, &s2);
        secp256k1_scalar_mul(&r1, &r1, &s);
        secp256k1_scalar_mul(&r2, &s1, &s);
        secp256k1_scalar_mul(&t, &s2, &s);
        secp256k1_scalar_add(&r2, &r2, &t);
        CHECK(secp256k1_scalar_eq(&r1, &r2));
    }

    {
        /* Test square. */
        secp256k1_scalar r1, r2;
        secp256k1_scalar_sqr(&r1, &s1);
        secp256k1_scalar_mul(&r2, &s1, &s1);
        CHECK(secp256k1_scalar_eq(&r1, &r2));
    }

    {
        /* Test multiplicative identity. */
        secp256k1_scalar r1, v1;
        secp256k1_scalar_set_int(&v1,1);
        secp256k1_scalar_mul(&r1, &s1, &v1);
        CHECK(secp256k1_scalar_eq(&r1, &s1));
    }

    {
        /* Test additive identity. */
        secp256k1_scalar r1, v0;
        secp256k1_scalar_set_int(&v0,0);
        secp256k1_scalar_add(&r1, &s1, &v0);
        CHECK(secp256k1_scalar_eq(&r1, &s1));
    }

    {
        /* Test zero product property. */
        secp256k1_scalar r1, v0;
        secp256k1_scalar_set_int(&v0,0);
        secp256k1_scalar_mul(&r1, &s1, &v0);
        CHECK(secp256k1_scalar_eq(&r1, &v0));
    }

}

void run_scalar_tests(void) {
    int i;
    for (i = 0; i < 128 * count; i++) {
        scalar_test();
    }

    {
        /* (-1)+1 should be zero. */
        secp256k1_scalar s, o;
        secp256k1_scalar_set_int(&s, 1);
        CHECK(secp256k1_scalar_is_one(&s));
        secp256k1_scalar_negate(&o, &s);
        secp256k1_scalar_add(&o, &o, &s);
        CHECK(secp256k1_scalar_is_zero(&o));
        secp256k1_scalar_negate(&o, &o);
        CHECK(secp256k1_scalar_is_zero(&o));
    }

#ifndef USE_NUM_NONE
    {
        /* A scalar with value of the curve order should be 0. */
        secp256k1_num order;
        secp256k1_scalar zero;
        unsigned char bin[32];
        int overflow = 0;
        secp256k1_scalar_order_get_num(&order);
        secp256k1_num_get_bin(bin, 32, &order);
        secp256k1_scalar_set_b32(&zero, bin, &overflow);
        CHECK(overflow == 1);
        CHECK(secp256k1_scalar_is_zero(&zero));
    }
#endif

    {
        /* Does check_overflow check catch all ones? */
        static const secp256k1_scalar overflowed = SECP256K1_SCALAR_CONST(
            0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL,
            0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL
        );
        CHECK(secp256k1_scalar_check_overflow(&overflowed));
    }

    {
        /* Static test vectors.
         * These were reduced from ~10^12 random vectors based on comparison-decision
         *  and edge-case coverage on 32-bit and 64-bit implementations.
         * The responses were generated with Sage 5.9.
         */
        secp256k1_scalar x;
        secp256k1_scalar y;
        secp256k1_scalar z;
        secp256k1_scalar zz;
        secp256k1_scalar one;
        secp256k1_scalar r1;
        secp256k1_scalar r2;
#if defined(USE_SCALAR_INV_NUM)
        secp256k1_scalar zzv;
#endif
        int overflow;
        unsigned char chal[33][2][32] = {
            {{0xff, 0xff, 0x03, 0x07, 0x00, 0x00, 0x00, 0x00,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03,
              0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff,
              0xff, 0xff, 0x03, 0x00, 0xc0, 0xff, 0xff, 0xff},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff}},
            {{0xef, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00,
              0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
             {0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0,
              0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x80, 0xff}},
            {{0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
              0x80, 0x00, 0x00, 0x80, 0xff, 0x3f, 0x00, 0x00,
              0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x00},
             {0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff, 0x80,
              0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x00, 0xe0,
              0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff}},
            {{0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
              0x00, 0x1e, 0xf8, 0xff, 0xff, 0xff, 0xfd, 0xff},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f,
              0x00, 0x00, 0x00, 0xf8, 0xff, 0x03, 0x00, 0xe0,
              0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff,
              0xf3, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {{0x80, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x00,
              0x00, 0x1c, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0x00,
              0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00,
              0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0x1f, 0x00, 0x00, 0x80, 0xff, 0xff, 0x3f,
              0x00, 0xfe, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xff}},
            {{0xff, 0xff, 0xff, 0xff, 0x00, 0x0f, 0xfc, 0x9f,
              0xff, 0xff, 0xff, 0x00, 0x80, 0x00, 0x00, 0x80,
              0xff, 0x0f, 0xfc, 0xff, 0x7f, 0x00, 0x00, 0x00,
              0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00},
             {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
              0x00, 0x00, 0xf8, 0xff, 0x0f, 0xc0, 0xff, 0xff,
              0xff, 0x1f, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff,
              0xff, 0xff, 0xff, 0x07, 0x80, 0xff, 0xff, 0xff}},
            {{0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00,
              0x80, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff,
              0xf7, 0xff, 0xff, 0xef, 0xff, 0xff, 0xff, 0x00,
              0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xf0},
             {0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
            {{0x00, 0xf8, 0xff, 0x03, 0xff, 0xff, 0xff, 0x00,
              0x00, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
              0x80, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0x03, 0xc0, 0xff, 0x0f, 0xfc, 0xff},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff,
              0xff, 0x01, 0x00, 0x00, 0x00, 0x3f, 0x00, 0xc0,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
            {{0x8f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0x7f, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {{0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0x03, 0x00, 0x80, 0x00, 0x00, 0x80,
              0xff, 0xff, 0xff, 0x00, 0x00, 0x80, 0xff, 0x7f},
             {0xff, 0xcf, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00,
              0x00, 0xc0, 0xff, 0xcf, 0xff, 0xff, 0xff, 0xff,
              0xbf, 0xff, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x80, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00}},
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff,
              0xff, 0xff, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0x00, 0x80, 0x00, 0x00, 0x80,
              0xff, 0x01, 0xfc, 0xff, 0x01, 0x00, 0xfe, 0xff},
             {0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00}},
            {{0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
              0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0x7f, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80},
             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0xf8, 0xff, 0x01, 0x00, 0xf0, 0xff, 0xff,
              0xe0, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x00},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
              0xfc, 0xff, 0xff, 0x3f, 0xf0, 0xff, 0xff, 0x3f,
              0x00, 0x00, 0xf8, 0x07, 0x00, 0x00, 0x00, 0xff,
              0xff, 0xff, 0xff, 0xff, 0x0f, 0x7e, 0x00, 0x00}},
            {{0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0x1f, 0x00, 0x00, 0xfe, 0x07, 0x00},
             {0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xfb, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60}},
            {{0xff, 0x01, 0x00, 0xff, 0xff, 0xff, 0x0f, 0x00,
              0x80, 0x7f, 0xfe, 0xff, 0xff, 0xff, 0xff, 0x03,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
             {0xff, 0xff, 0x1f, 0x00, 0xf0, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x00}},
            {{0x80, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03,
              0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff}},
            {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
              0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0xc0, 0xff, 0xff, 0xcf, 0xff, 0x1f, 0x00, 0x00,
              0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x7e,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00},
             {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
              0xff, 0xff, 0x7f, 0x00, 0x80, 0x00, 0x00, 0x00,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
              0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff}},
            {{0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x80,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
              0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00},
             {0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x80,
              0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
              0xff, 0x7f, 0xf8, 0xff, 0xff, 0x1f, 0x00, 0xfe}},
            {{0xff, 0xff, 0xff, 0x3f, 0xf8, 0xff, 0xff, 0xff,
              0xff, 0x03, 0xfe, 0x01, 0x00, 0x00, 0x00, 0x00,
              0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
              0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
              0xff, 0xff, 0xff, 0xff, 0x01, 0x80, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00}},
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
              0xba, 0xae, 0xdc, 0xe6, 0xaf, 0x48, 0xa0, 0x3b,
              0xbf, 0xd2, 0x5e, 0x8c, 0xd0, 0x36, 0x41, 0x40}},
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {{0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
             {0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
            {{0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xc0,
              0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00,
              0xf0, 0xff, 0xff, 0xff, 0xff, 0x07, 0x00, 0x00,
              0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff}},
            {{0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}},
            {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
              0xba, 0xae, 0xdc, 0xe6, 0xaf, 0x48, 0xa0, 0x3b,
              0xbf, 0xd2, 0x5e, 0x8c, 0xd0, 0x36, 0x41, 0x40},
             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}},
            {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0x7e, 0x00, 0x00, 0xc0, 0xff, 0xff, 0x07, 0x00,
              0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
              0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
             {0xff, 0x01, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x80,
              0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
            {{0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x00,
              0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
              0x00, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01,
              0x80, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff},
             {0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff,
              0xff, 0xff, 0x3f, 0x00, 0xf8, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0x3f, 0x00, 0x00, 0xc0, 0xf1, 0x7f, 0x00}},
            {{0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x80, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x00},
             {0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff,
              0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x80, 0x1f,
              0x00, 0x00, 0xfc, 0xff, 0xff, 0x01, 0xff, 0xff}},
            {{0x00, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
              0x80, 0x00, 0x00, 0x80, 0xff, 0x03, 0xe0, 0x01,
              0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xfc, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00},
             {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
              0xfe, 0xff, 0xff, 0xf0, 0x07, 0x00, 0x3c, 0x80,
              0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff,
              0xff, 0xff, 0x07, 0xe0, 0xff, 0x00, 0x00, 0x00}},
            {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
              0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xf8,
              0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80},
             {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0x0c, 0x80, 0x00,
              0x00, 0x00, 0x00, 0xc0, 0x7f, 0xfe, 0xff, 0x1f,
              0x00, 0xfe, 0xff, 0x03, 0x00, 0x00, 0xfe, 0xff}},
            {{0xff, 0xff, 0x81, 0xff, 0xff, 0xff, 0xff, 0x00,
              0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x83,
              0xff, 0xff, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
              0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0xf0},
             {0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x00,
              0xf8, 0x07, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xc7, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff}},
            {{0x82, 0xc9, 0xfa, 0xb0, 0x68, 0x04, 0xa0, 0x00,
              0x82, 0xc9, 0xfa, 0xb0, 0x68, 0x04, 0xa0, 0x00,
              0xff, 0xff, 0xff, 0xff, 0xff, 0x6f, 0x03, 0xfb,
              0xfa, 0x8a, 0x7d, 0xdf, 0x13, 0x86, 0xe2, 0x03},
             {0x82, 0xc9, 0xfa, 0xb0, 0x68, 0x04, 0xa0, 0x00,
              0x82, 0xc9, 0xfa, 0xb0, 0x68, 0x04, 0xa0, 0x00,
              0xff, 0xff, 0xff, 0xff, 0xff, 0x6f, 0x03, 0xfb,
              0xfa, 0x8a, 0x7d, 0xdf, 0x13, 0x86, 0xe2, 0x03}}
        };
        unsigned char res[33][2][32] = {
            {{0x0c, 0x3b, 0x0a, 0xca, 0x8d, 0x1a, 0x2f, 0xb9,
              0x8a, 0x7b, 0x53, 0x5a, 0x1f, 0xc5, 0x22, 0xa1,
              0x07, 0x2a, 0x48, 0xea, 0x02, 0xeb, 0xb3, 0xd6,
              0x20, 0x1e, 0x86, 0xd0, 0x95, 0xf6, 0x92, 0x35},
             {0xdc, 0x90, 0x7a, 0x07, 0x2e, 0x1e, 0x44, 0x6d,
              0xf8, 0x15, 0x24, 0x5b, 0x5a, 0x96, 0x37, 0x9c,
              0x37, 0x7b, 0x0d, 0xac, 0x1b, 0x65, 0x58, 0x49,
              0x43, 0xb7, 0x31, 0xbb, 0xa7, 0xf4, 0x97, 0x15}},
            {{0xf1, 0xf7, 0x3a, 0x50, 0xe6, 0x10, 0xba, 0x22,
              0x43, 0x4d, 0x1f, 0x1f, 0x7c, 0x27, 0xca, 0x9c,
              0xb8, 0xb6, 0xa0, 0xfc, 0xd8, 0xc0, 0x05, 0x2f,
              0xf7, 0x08, 0xe1, 0x76, 0xdd, 0xd0, 0x80, 0xc8},
             {0xe3, 0x80, 0x80, 0xb8, 0xdb, 0xe3, 0xa9, 0x77,
              0x00, 0xb0, 0xf5, 0x2e, 0x27, 0xe2, 0x68, 0xc4,
              0x88, 0xe8, 0x04, 0xc1, 0x12, 0xbf, 0x78, 0x59,
              0xe6, 0xa9, 0x7c, 0xe1, 0x81, 0xdd, 0xb9, 0xd5}},
            {{0x96, 0xe2, 0xee, 0x01, 0xa6, 0x80, 0x31, 0xef,
              0x5c, 0xd0, 0x19, 0xb4, 0x7d, 0x5f, 0x79, 0xab,
              0xa1, 0x97, 0xd3, 0x7e, 0x33, 0xbb, 0x86, 0x55,
              0x60, 0x20, 0x10, 0x0d, 0x94, 0x2d, 0x11, 0x7c},
             {0xcc, 0xab, 0xe0, 0xe8, 0x98, 0x65, 0x12, 0x96,
              0x38, 0x5a, 0x1a, 0xf2, 0x85, 0x23, 0x59, 0x5f,
              0xf9, 0xf3, 0xc2, 0x81, 0x70, 0x92, 0x65, 0x12,
              0x9c, 0x65, 0x1e, 0x96, 0x00, 0xef, 0xe7, 0x63}},
            {{0xac, 0x1e, 0x62, 0xc2, 0x59, 0xfc, 0x4e, 0x5c,
              0x83, 0xb0, 0xd0, 0x6f, 0xce, 0x19, 0xf6, 0xbf,
              0xa4, 0xb0, 0xe0, 0x53, 0x66, 0x1f, 0xbf, 0xc9,
              0x33, 0x47, 0x37, 0xa9, 0x3d, 0x5d, 0xb0, 0x48},
             {0x86, 0xb9, 0x2a, 0x7f, 0x8e, 0xa8, 0x60, 0x42,
              0x26, 0x6d, 0x6e, 0x1c, 0xa2, 0xec, 0xe0, 0xe5,
              0x3e, 0x0a, 0x33, 0xbb, 0x61, 0x4c, 0x9f, 0x3c,
              0xd1, 0xdf, 0x49, 0x33, 0xcd, 0x72, 0x78, 0x18}},
            {{0xf7, 0xd3, 0xcd, 0x49, 0x5c, 0x13, 0x22, 0xfb,
              0x2e, 0xb2, 0x2f, 0x27, 0xf5, 0x8a, 0x5d, 0x74,
              0xc1, 0x58, 0xc5, 0xc2, 0x2d, 0x9f, 0x52, 0xc6,
              0x63, 0x9f, 0xba, 0x05, 0x76, 0x45, 0x7a, 0x63},
             {0x8a, 0xfa, 0x55, 0x4d, 0xdd, 0xa3, 0xb2, 0xc3,
              0x44, 0xfd, 0xec, 0x72, 0xde, 0xef, 0xc0, 0x99,
              0xf5, 0x9f, 0xe2, 0x52, 0xb4, 0x05, 0x32, 0x58,
              0x57, 0xc1, 0x8f, 0xea, 0xc3, 0x24, 0x5b, 0x94}},
            {{0x05, 0x83, 0xee, 0xdd, 0x64, 0xf0, 0x14, 0x3b,
              0xa0, 0x14, 0x4a, 0x3a, 0x41, 0x82, 0x7c, 0xa7,
              0x2c, 0xaa, 0xb1, 0x76, 0xbb, 0x59, 0x64, 0x5f,
              0x52, 0xad, 0x25, 0x29, 0x9d, 0x8f, 0x0b, 0xb0},
             {0x7e, 0xe3, 0x7c, 0xca, 0xcd, 0x4f, 0xb0, 0x6d,
              0x7a, 0xb2, 0x3e, 0xa0, 0x08, 0xb9, 0xa8, 0x2d,
              0xc2, 0xf4, 0x99, 0x66, 0xcc, 0xac, 0xd8, 0xb9,
              0x72, 0x2a, 0x4a, 0x3e, 0x0f, 0x7b, 0xbf, 0xf4}},
            {{0x8c, 0x9c, 0x78, 0x2b, 0x39, 0x61, 0x7e, 0xf7,
              0x65, 0x37, 0x66, 0x09, 0x38, 0xb9, 0x6f, 0x70,
              0x78, 0x87, 0xff, 0xcf, 0x93, 0xca, 0x85, 0x06,
              0x44, 0x84, 0xa7, 0xfe, 0xd3, 0xa4, 0xe3, 0x7e},
             {0xa2, 0x56, 0x49, 0x23, 0x54, 0xa5, 0x50, 0xe9,
              0x5f, 0xf0, 0x4d, 0xe7, 0xdc, 0x38, 0x32, 0x79,
              0x4f, 0x1c, 0xb7, 0xe4, 0xbb, 0xf8, 0xbb, 0x2e,
              0x40, 0x41, 0x4b, 0xcc, 0xe3, 0x1e, 0x16, 0x36}},
            {{0x0c, 0x1e, 0xd7, 0x09, 0x25, 0x40, 0x97, 0xcb,
              0x5c, 0x46, 0xa8, 0xda, 0xef, 0x25, 0xd5, 0xe5,
              0x92, 0x4d, 0xcf, 0xa3, 0xc4, 0x5d, 0x35, 0x4a,
              0xe4, 0x61, 0x92, 0xf3, 0xbf, 0x0e, 0xcd, 0xbe},
             {0xe4, 0xaf, 0x0a, 0xb3, 0x30, 0x8b, 0x9b, 0x48,
              0x49, 0x43, 0xc7, 0x64, 0x60, 0x4a, 0x2b, 0x9e,
              0x95, 0x5f, 0x56, 0xe8, 0x35, 0xdc, 0xeb, 0xdc,
              0xc7, 0xc4, 0xfe, 0x30, 0x40, 0xc7, 0xbf, 0xa4}},
            {{0xd4, 0xa0, 0xf5, 0x81, 0x49, 0x6b, 0xb6, 0x8b,
              0x0a, 0x69, 0xf9, 0xfe, 0xa8, 0x32, 0xe5, 0xe0,
              0xa5, 0xcd, 0x02, 0x53, 0xf9, 0x2c, 0xe3, 0x53,
              0x83, 0x36, 0xc6, 0x02, 0xb5, 0xeb, 0x64, 0xb8},
             {0x1d, 0x42, 0xb9, 0xf9, 0xe9, 0xe3, 0x93, 0x2c,
              0x4c, 0xee, 0x6c, 0x5a, 0x47, 0x9e, 0x62, 0x01,
              0x6b, 0x04, 0xfe, 0xa4, 0x30, 0x2b, 0x0d, 0x4f,
              0x71, 0x10, 0xd3, 0x55, 0xca, 0xf3, 0x5e, 0x80}},
            {{0x77, 0x05, 0xf6, 0x0c, 0x15, 0x9b, 0x45, 0xe7,
              0xb9, 0x11, 0xb8, 0xf5, 0xd6, 0xda, 0x73, 0x0c,
              0xda, 0x92, 0xea, 0xd0, 0x9d, 0xd0, 0x18, 0x92,
              0xce, 0x9a, 0xaa, 0xee, 0x0f, 0xef, 0xde, 0x30},
             {0xf1, 0xf1, 0xd6, 0x9b, 0x51, 0xd7, 0x77, 0x62,
              0x52, 0x10, 0xb8, 0x7a, 0x84, 0x9d, 0x15, 0x4e,
              0x07, 0xdc, 0x1e, 0x75, 0x0d, 0x0c, 0x3b, 0xdb,
              0x74, 0x58, 0x62, 0x02, 0x90, 0x54, 0x8b, 0x43}},
            {{0xa6, 0xfe, 0x0b, 0x87, 0x80, 0x43, 0x67, 0x25,
              0x57, 0x5d, 0xec, 0x40, 0x50, 0x08, 0xd5, 0x5d,
              0x43, 0xd7, 0xe0, 0xaa, 0xe0, 0x13, 0xb6, 0xb0,
              0xc0, 0xd4, 0xe5, 0x0d, 0x45, 0x83, 0xd6, 0x13},
             {0x40, 0x45, 0x0a, 0x92, 0x31, 0xea, 0x8c, 0x60,
              0x8c, 0x1f, 0xd8, 0x76, 0x45, 0xb9, 0x29, 0x00,
              0x26, 0x32, 0xd8, 0xa6, 0x96, 0x88, 0xe2, 0xc4,
              0x8b, 0xdb, 0x7f, 0x17, 0x87, 0xcc, 0xc8, 0xf2}},
            {{0xc2, 0x56, 0xe2, 0xb6, 0x1a, 0x81, 0xe7, 0x31,
              0x63, 0x2e, 0xbb, 0x0d, 0x2f, 0x81, 0x67, 0xd4,
              0x22, 0xe2, 0x38, 0x02, 0x25, 0x97, 0xc7, 0x88,
              0x6e, 0xdf, 0xbe, 0x2a, 0xa5, 0x73, 0x63, 0xaa},
             {0x50, 0x45, 0xe2, 0xc3, 0xbd, 0x89, 0xfc, 0x57,
              0xbd, 0x3c, 0xa3, 0x98, 0x7e, 0x7f, 0x36, 0x38,
              0x92, 0x39, 0x1f, 0x0f, 0x81, 0x1a, 0x06, 0x51,
              0x1f, 0x8d, 0x6a, 0xff, 0x47, 0x16, 0x06, 0x9c}},
            {{0x33, 0x95, 0xa2, 0x6f, 0x27, 0x5f, 0x9c, 0x9c,
              0x64, 0x45, 0xcb, 0xd1, 0x3c, 0xee, 0x5e, 0x5f,
              0x48, 0xa6, 0xaf, 0xe3, 0x79, 0xcf, 0xb1, 0xe2,
              0xbf, 0x55, 0x0e, 0xa2, 0x3b, 0x62, 0xf0, 0xe4},
             {0x14, 0xe8, 0x06, 0xe3, 0xbe, 0x7e, 0x67, 0x01,
              0xc5, 0x21, 0x67, 0xd8, 0x54, 0xb5, 0x7f, 0xa4,
              0xf9, 0x75, 0x70, 0x1c, 0xfd, 0x79, 0xdb, 0x86,
              0xad, 0x37, 0x85, 0x83, 0x56, 0x4e, 0xf0, 0xbf}},
            {{0xbc, 0xa6, 0xe0, 0x56, 0x4e, 0xef, 0xfa, 0xf5,
              0x1d, 0x5d, 0x3f, 0x2a, 0x5b, 0x19, 0xab, 0x51,
              0xc5, 0x8b, 0xdd, 0x98, 0x28, 0x35, 0x2f, 0xc3,
              0x81, 0x4f, 0x5c, 0xe5, 0x70, 0xb9, 0xeb, 0x62},
             {0xc4, 0x6d, 0x26, 0xb0, 0x17, 0x6b, 0xfe, 0x6c,
              0x12, 0xf8, 0xe7, 0xc1, 0xf5, 0x2f, 0xfa, 0x91,
              0x13, 0x27, 0xbd, 0x73, 0xcc, 0x33, 0x31, 0x1c,
              0x39, 0xe3, 0x27, 0x6a, 0x95, 0xcf, 0xc5, 0xfb}},
            {{0x30, 0xb2, 0x99, 0x84, 0xf0, 0x18, 0x2a, 0x6e,
              0x1e, 0x27, 0xed, 0xa2, 0x29, 0x99, 0x41, 0x56,
              0xe8, 0xd4, 0x0d, 0xef, 0x99, 0x9c, 0xf3, 0x58,
              0x29, 0x55, 0x1a, 0xc0, 0x68, 0xd6, 0x74, 0xa4},
             {0x07, 0x9c, 0xe7, 0xec, 0xf5, 0x36, 0x73, 0x41,
              0xa3, 0x1c, 0xe5, 0x93, 0x97, 0x6a, 0xfd, 0xf7,
              0x53, 0x18, 0xab, 0xaf, 0xeb, 0x85, 0xbd, 0x92,
              0x90, 0xab, 0x3c, 0xbf, 0x30, 0x82, 0xad, 0xf6}},
            {{0xc6, 0x87, 0x8a, 0x2a, 0xea, 0xc0, 0xa9, 0xec,
              0x6d, 0xd3, 0xdc, 0x32, 0x23, 0xce, 0x62, 0x19,
              0xa4, 0x7e, 0xa8, 0xdd, 0x1c, 0x33, 0xae, 0xd3,
              0x4f, 0x62, 0x9f, 0x52, 0xe7, 0x65, 0x46, 0xf4},
             {0x97, 0x51, 0x27, 0x67, 0x2d, 0xa2, 0x82, 0x87,
              0x98, 0xd3, 0xb6, 0x14, 0x7f, 0x51, 0xd3, 0x9a,
              0x0b, 0xd0, 0x76, 0x81, 0xb2, 0x4f, 0x58, 0x92,
              0xa4, 0x86, 0xa1, 0xa7, 0x09, 0x1d, 0xef, 0x9b}},
            {{0xb3, 0x0f, 0x2b, 0x69, 0x0d, 0x06, 0x90, 0x64,
              0xbd, 0x43, 0x4c, 0x10, 0xe8, 0x98, 0x1c, 0xa3,
              0xe1, 0x68, 0xe9, 0x79, 0x6c, 0x29, 0x51, 0x3f,
              0x41, 0xdc, 0xdf, 0x1f, 0xf3, 0x60, 0xbe, 0x33},
             {0xa1, 0x5f, 0xf7, 0x1d, 0xb4, 0x3e, 0x9b, 0x3c,
              0xe7, 0xbd, 0xb6, 0x06, 0xd5, 0x60, 0x06, 0x6d,
              0x50, 0xd2, 0xf4, 0x1a, 0x31, 0x08, 0xf2, 0xea,
              0x8e, 0xef, 0x5f, 0x7d, 0xb6, 0xd0, 0xc0, 0x27}},
            {{0x62, 0x9a, 0xd9, 0xbb, 0x38, 0x36, 0xce, 0xf7,
              0x5d, 0x2f, 0x13, 0xec, 0xc8, 0x2d, 0x02, 0x8a,
              0x2e, 0x72, 0xf0, 0xe5, 0x15, 0x9d, 0x72, 0xae,
              0xfc, 0xb3, 0x4f, 0x02, 0xea, 0xe1, 0x09, 0xfe},
             {0x00, 0x00, 0x00, 0x00, 0xfa, 0x0a, 0x3d, 0xbc,
              0xad, 0x16, 0x0c, 0xb6, 0xe7, 0x7c, 0x8b, 0x39,
              0x9a, 0x43, 0xbb, 0xe3, 0xc2, 0x55, 0x15, 0x14,
              0x75, 0xac, 0x90, 0x9b, 0x7f, 0x9a, 0x92, 0x00}},
            {{0x8b, 0xac, 0x70, 0x86, 0x29, 0x8f, 0x00, 0x23,
              0x7b, 0x45, 0x30, 0xaa, 0xb8, 0x4c, 0xc7, 0x8d,
              0x4e, 0x47, 0x85, 0xc6, 0x19, 0xe3, 0x96, 0xc2,
              0x9a, 0xa0, 0x12, 0xed, 0x6f, 0xd7, 0x76, 0x16},
             {0x45, 0xaf, 0x7e, 0x33, 0xc7, 0x7f, 0x10, 0x6c,
              0x7c, 0x9f, 0x29, 0xc1, 0xa8, 0x7e, 0x15, 0x84,
              0xe7, 0x7d, 0xc0, 0x6d, 0xab, 0x71, 0x5d, 0xd0,
              0x6b, 0x9f, 0x97, 0xab, 0xcb, 0x51, 0x0c, 0x9f}},
            {{0x9e, 0xc3, 0x92, 0xb4, 0x04, 0x9f, 0xc8, 0xbb,
              0xdd, 0x9e, 0xc6, 0x05, 0xfd, 0x65, 0xec, 0x94,
              0x7f, 0x2c, 0x16, 0xc4, 0x40, 0xac, 0x63, 0x7b,
              0x7d, 0xb8, 0x0c, 0xe4, 0x5b, 0xe3, 0xa7, 0x0e},
             {0x43, 0xf4, 0x44, 0xe8, 0xcc, 0xc8, 0xd4, 0x54,
              0x33, 0x37, 0x50, 0xf2, 0x87, 0x42, 0x2e, 0x00,
              0x49, 0x60, 0x62, 0x02, 0xfd, 0x1a, 0x7c, 0xdb,
              0x29, 0x6c, 0x6d, 0x54, 0x53, 0x08, 0xd1, 0xc8}},
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}},
            {{0x27, 0x59, 0xc7, 0x35, 0x60, 0x71, 0xa6, 0xf1,
              0x79, 0xa5, 0xfd, 0x79, 0x16, 0xf3, 0x41, 0xf0,
              0x57, 0xb4, 0x02, 0x97, 0x32, 0xe7, 0xde, 0x59,
              0xe2, 0x2d, 0x9b, 0x11, 0xea, 0x2c, 0x35, 0x92},
             {0x27, 0x59, 0xc7, 0x35, 0x60, 0x71, 0xa6, 0xf1,
              0x79, 0xa5, 0xfd, 0x79, 0x16, 0xf3, 0x41, 0xf0,
              0x57, 0xb4, 0x02, 0x97, 0x32, 0xe7, 0xde, 0x59,
              0xe2, 0x2d, 0x9b, 0x11, 0xea, 0x2c, 0x35, 0x92}},
            {{0x28, 0x56, 0xac, 0x0e, 0x4f, 0x98, 0x09, 0xf0,
              0x49, 0xfa, 0x7f, 0x84, 0xac, 0x7e, 0x50, 0x5b,
              0x17, 0x43, 0x14, 0x89, 0x9c, 0x53, 0xa8, 0x94,
              0x30, 0xf2, 0x11, 0x4d, 0x92, 0x14, 0x27, 0xe8},
             {0x39, 0x7a, 0x84, 0x56, 0x79, 0x9d, 0xec, 0x26,
              0x2c, 0x53, 0xc1, 0x94, 0xc9, 0x8d, 0x9e, 0x9d,
              0x32, 0x1f, 0xdd, 0x84, 0x04, 0xe8, 0xe2, 0x0a,
              0x6b, 0xbe, 0xbb, 0x42, 0x40, 0x67, 0x30, 0x6c}},
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
              0x45, 0x51, 0x23, 0x19, 0x50, 0xb7, 0x5f, 0xc4,
              0x40, 0x2d, 0xa1, 0x73, 0x2f, 0xc9, 0xbe, 0xbd},
             {0x27, 0x59, 0xc7, 0x35, 0x60, 0x71, 0xa6, 0xf1,
              0x79, 0xa5, 0xfd, 0x79, 0x16, 0xf3, 0x41, 0xf0,
              0x57, 0xb4, 0x02, 0x97, 0x32, 0xe7, 0xde, 0x59,
              0xe2, 0x2d, 0x9b, 0x11, 0xea, 0x2c, 0x35, 0x92}},
            {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
              0xba, 0xae, 0xdc, 0xe6, 0xaf, 0x48, 0xa0, 0x3b,
              0xbf, 0xd2, 0x5e, 0x8c, 0xd0, 0x36, 0x41, 0x40},
             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}},
            {{0x1c, 0xc4, 0xf7, 0xda, 0x0f, 0x65, 0xca, 0x39,
              0x70, 0x52, 0x92, 0x8e, 0xc3, 0xc8, 0x15, 0xea,
              0x7f, 0x10, 0x9e, 0x77, 0x4b, 0x6e, 0x2d, 0xdf,
              0xe8, 0x30, 0x9d, 0xda, 0xe8, 0x9a, 0x65, 0xae},
             {0x02, 0xb0, 0x16, 0xb1, 0x1d, 0xc8, 0x57, 0x7b,
              0xa2, 0x3a, 0xa2, 0xa3, 0x38, 0x5c, 0x8f, 0xeb,
              0x66, 0x37, 0x91, 0xa8, 0x5f, 0xef, 0x04, 0xf6,
              0x59, 0x75, 0xe1, 0xee, 0x92, 0xf6, 0x0e, 0x30}},
            {{0x8d, 0x76, 0x14, 0xa4, 0x14, 0x06, 0x9f, 0x9a,
              0xdf, 0x4a, 0x85, 0xa7, 0x6b, 0xbf, 0x29, 0x6f,
              0xbc, 0x34, 0x87, 0x5d, 0xeb, 0xbb, 0x2e, 0xa9,
              0xc9, 0x1f, 0x58, 0xd6, 0x9a, 0x82, 0xa0, 0x56},
             {0xd4, 0xb9, 0xdb, 0x88, 0x1d, 0x04, 0xe9, 0x93,
              0x8d, 0x3f, 0x20, 0xd5, 0x86, 0xa8, 0x83, 0x07,
              0xdb, 0x09, 0xd8, 0x22, 0x1f, 0x7f, 0xf1, 0x71,
              0xc8, 0xe7, 0x5d, 0x47, 0xaf, 0x8b, 0x72, 0xe9}},
            {{0x83, 0xb9, 0x39, 0xb2, 0xa4, 0xdf, 0x46, 0x87,
              0xc2, 0xb8, 0xf1, 0xe6, 0x4c, 0xd1, 0xe2, 0xa9,
              0xe4, 0x70, 0x30, 0x34, 0xbc, 0x52, 0x7c, 0x55,
              0xa6, 0xec, 0x80, 0xa4, 0xe5, 0xd2, 0xdc, 0x73},
             {0x08, 0xf1, 0x03, 0xcf, 0x16, 0x73, 0xe8, 0x7d,
              0xb6, 0x7e, 0x9b, 0xc0, 0xb4, 0xc2, 0xa5, 0x86,
              0x02, 0x77, 0xd5, 0x27, 0x86, 0xa5, 0x15, 0xfb,
              0xae, 0x9b, 0x8c, 0xa9, 0xf9, 0xf8, 0xa8, 0x4a}},
            {{0x8b, 0x00, 0x49, 0xdb, 0xfa, 0xf0, 0x1b, 0xa2,
              0xed, 0x8a, 0x9a, 0x7a, 0x36, 0x78, 0x4a, 0xc7,
              0xf7, 0xad, 0x39, 0xd0, 0x6c, 0x65, 0x7a, 0x41,
              0xce, 0xd6, 0xd6, 0x4c, 0x20, 0x21, 0x6b, 0xc7},
             {0xc6, 0xca, 0x78, 0x1d, 0x32, 0x6c, 0x6c, 0x06,
              0x91, 0xf2, 0x1a, 0xe8, 0x43, 0x16, 0xea, 0x04,
              0x3c, 0x1f, 0x07, 0x85, 0xf7, 0x09, 0x22, 0x08,
              0xba, 0x13, 0xfd, 0x78, 0x1e, 0x3f, 0x6f, 0x62}},
            {{0x25, 0x9b, 0x7c, 0xb0, 0xac, 0x72, 0x6f, 0xb2,
              0xe3, 0x53, 0x84, 0x7a, 0x1a, 0x9a, 0x98, 0x9b,
              0x44, 0xd3, 0x59, 0xd0, 0x8e, 0x57, 0x41, 0x40,
              0x78, 0xa7, 0x30, 0x2f, 0x4c, 0x9c, 0xb9, 0x68},
             {0xb7, 0x75, 0x03, 0x63, 0x61, 0xc2, 0x48, 0x6e,
              0x12, 0x3d, 0xbf, 0x4b, 0x27, 0xdf, 0xb1, 0x7a,
              0xff, 0x4e, 0x31, 0x07, 0x83, 0xf4, 0x62, 0x5b,
              0x19, 0xa5, 0xac, 0xa0, 0x32, 0x58, 0x0d, 0xa7}},
            {{0x43, 0x4f, 0x10, 0xa4, 0xca, 0xdb, 0x38, 0x67,
              0xfa, 0xae, 0x96, 0xb5, 0x6d, 0x97, 0xff, 0x1f,
              0xb6, 0x83, 0x43, 0xd3, 0xa0, 0x2d, 0x70, 0x7a,
              0x64, 0x05, 0x4c, 0xa7, 0xc1, 0xa5, 0x21, 0x51},
             {0xe4, 0xf1, 0x23, 0x84, 0xe1, 0xb5, 0x9d, 0xf2,
              0xb8, 0x73, 0x8b, 0x45, 0x2b, 0x35, 0x46, 0x38,
              0x10, 0x2b, 0x50, 0xf8, 0x8b, 0x35, 0xcd, 0x34,
              0xc8, 0x0e, 0xf6, 0xdb, 0x09, 0x35, 0xf0, 0xda}},
            {{0xdb, 0x21, 0x5c, 0x8d, 0x83, 0x1d, 0xb3, 0x34,
              0xc7, 0x0e, 0x43, 0xa1, 0x58, 0x79, 0x67, 0x13,
              0x1e, 0x86, 0x5d, 0x89, 0x63, 0xe6, 0x0a, 0x46,
              0x5c, 0x02, 0x97, 0x1b, 0x62, 0x43, 0x86, 0xf5},
             {0xdb, 0x21, 0x5c, 0x8d, 0x83, 0x1d, 0xb3, 0x34,
              0xc7, 0x0e, 0x43, 0xa1, 0x58, 0x79, 0x67, 0x13,
              0x1e, 0x86, 0x5d, 0x89, 0x63, 0xe6, 0x0a, 0x46,
              0x5c, 0x02, 0x97, 0x1b, 0x62, 0x43, 0x86, 0xf5}}
        };
        secp256k1_scalar_set_int(&one, 1);
        for (i = 0; i < 33; i++) {
            secp256k1_scalar_set_b32(&x, chal[i][0], &overflow);
            CHECK(!overflow);
            secp256k1_scalar_set_b32(&y, chal[i][1], &overflow);
            CHECK(!overflow);
            secp256k1_scalar_set_b32(&r1, res[i][0], &overflow);
            CHECK(!overflow);
            secp256k1_scalar_set_b32(&r2, res[i][1], &overflow);
            CHECK(!overflow);
            secp256k1_scalar_mul(&z, &x, &y);
            CHECK(!secp256k1_scalar_check_overflow(&z));
            CHECK(secp256k1_scalar_eq(&r1, &z));
            if (!secp256k1_scalar_is_zero(&y)) {
                secp256k1_scalar_inverse(&zz, &y);
                CHECK(!secp256k1_scalar_check_overflow(&zz));
#if defined(USE_SCALAR_INV_NUM)
                secp256k1_scalar_inverse_var(&zzv, &y);
                CHECK(secp256k1_scalar_eq(&zzv, &zz));
#endif
                secp256k1_scalar_mul(&z, &z, &zz);
                CHECK(!secp256k1_scalar_check_overflow(&z));
                CHECK(secp256k1_scalar_eq(&x, &z));
                secp256k1_scalar_mul(&zz, &zz, &y);
                CHECK(!secp256k1_scalar_check_overflow(&zz));
                CHECK(secp256k1_scalar_eq(&one, &zz));
            }
            secp256k1_scalar_mul(&z, &x, &x);
            CHECK(!secp256k1_scalar_check_overflow(&z));
            secp256k1_scalar_sqr(&zz, &x);
            CHECK(!secp256k1_scalar_check_overflow(&zz));
            CHECK(secp256k1_scalar_eq(&zz, &z));
            CHECK(secp256k1_scalar_eq(&r2, &zz));
        }
    }
}

/***** FIELD TESTS *****/

void random_fe(secp256k1_fe *x) {
    unsigned char bin[32];
    do {
        secp256k1_rand256(bin);
        if (secp256k1_fe_set_b32(x, bin)) {
            return;
        }
    } while(1);
}

void random_fe_test(secp256k1_fe *x) {
    unsigned char bin[32];
    do {
        secp256k1_rand256_test(bin);
        if (secp256k1_fe_set_b32(x, bin)) {
            return;
        }
    } while(1);
}

void random_fe_non_zero(secp256k1_fe *nz) {
    int tries = 10;
    while (--tries >= 0) {
        random_fe(nz);
        secp256k1_fe_normalize(nz);
        if (!secp256k1_fe_is_zero(nz)) {
            break;
        }
    }
    /* Infinitesimal probability of spurious failure here */
    CHECK(tries >= 0);
}

void random_fe_non_square(secp256k1_fe *ns) {
    secp256k1_fe r;
    random_fe_non_zero(ns);
    if (secp256k1_fe_sqrt(&r, ns)) {
        secp256k1_fe_negate(ns, ns, 1);
    }
}

int check_fe_equal(const secp256k1_fe *a, const secp256k1_fe *b) {
    secp256k1_fe an = *a;
    secp256k1_fe bn = *b;
    secp256k1_fe_normalize_weak(&an);
    secp256k1_fe_normalize_var(&bn);
    return secp256k1_fe_equal_var(&an, &bn);
}

int check_fe_inverse(const secp256k1_fe *a, const secp256k1_fe *ai) {
    secp256k1_fe x;
    secp256k1_fe one = SECP256K1_FE_CONST(0, 0, 0, 0, 0, 0, 0, 1);
    secp256k1_fe_mul(&x, a, ai);
    return check_fe_equal(&x, &one);
}

void run_field_convert(void) {
    static const unsigned char b32[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x40
    };
    static const secp256k1_fe_storage fes = SECP256K1_FE_STORAGE_CONST(
        0x00010203UL, 0x04050607UL, 0x11121314UL, 0x15161718UL,
        0x22232425UL, 0x26272829UL, 0x33343536UL, 0x37383940UL
    );
    static const secp256k1_fe fe = SECP256K1_FE_CONST(
        0x00010203UL, 0x04050607UL, 0x11121314UL, 0x15161718UL,
        0x22232425UL, 0x26272829UL, 0x33343536UL, 0x37383940UL
    );
    secp256k1_fe fe2;
    unsigned char b322[32];
    secp256k1_fe_storage fes2;
    /* Check conversions to fe. */
    CHECK(secp256k1_fe_set_b32(&fe2, b32));
    CHECK(secp256k1_fe_equal_var(&fe, &fe2));
    secp256k1_fe_from_storage(&fe2, &fes);
    CHECK(secp256k1_fe_equal_var(&fe, &fe2));
    /* Check conversion from fe. */
    secp256k1_fe_get_b32(b322, &fe);
    CHECK(memcmp(b322, b32, 32) == 0);
    secp256k1_fe_to_storage(&fes2, &fe);
    CHECK(memcmp(&fes2, &fes, sizeof(fes)) == 0);
}

int fe_memcmp(const secp256k1_fe *a, const secp256k1_fe *b) {
    secp256k1_fe t = *b;
#ifdef VERIFY
    t.magnitude = a->magnitude;
    t.normalized = a->normalized;
#endif
    return memcmp(a, &t, sizeof(secp256k1_fe));
}

void run_field_misc(void) {
    secp256k1_fe x;
    secp256k1_fe y;
    secp256k1_fe z;
    secp256k1_fe q;
    secp256k1_fe fe5 = SECP256K1_FE_CONST(0, 0, 0, 0, 0, 0, 0, 5);
    int i, j;
    for (i = 0; i < 5*count; i++) {
        secp256k1_fe_storage xs, ys, zs;
        random_fe(&x);
        random_fe_non_zero(&y);
        /* Test the fe equality and comparison operations. */
        CHECK(secp256k1_fe_cmp_var(&x, &x) == 0);
        CHECK(secp256k1_fe_equal_var(&x, &x));
        z = x;
        secp256k1_fe_add(&z,&y);
        /* Test fe conditional move; z is not normalized here. */
        q = x;
        secp256k1_fe_cmov(&x, &z, 0);
        VERIFY_CHECK(!x.normalized && x.magnitude == z.magnitude);
        secp256k1_fe_cmov(&x, &x, 1);
        CHECK(fe_memcmp(&x, &z) != 0);
        CHECK(fe_memcmp(&x, &q) == 0);
        secp256k1_fe_cmov(&q, &z, 1);
        VERIFY_CHECK(!q.normalized && q.magnitude == z.magnitude);
        CHECK(fe_memcmp(&q, &z) == 0);
        secp256k1_fe_normalize_var(&x);
        secp256k1_fe_normalize_var(&z);
        CHECK(!secp256k1_fe_equal_var(&x, &z));
        secp256k1_fe_normalize_var(&q);
        secp256k1_fe_cmov(&q, &z, (i&1));
        VERIFY_CHECK(q.normalized && q.magnitude == 1);
        for (j = 0; j < 6; j++) {
            secp256k1_fe_negate(&z, &z, j+1);
            secp256k1_fe_normalize_var(&q);
            secp256k1_fe_cmov(&q, &z, (j&1));
            VERIFY_CHECK(!q.normalized && q.magnitude == (j+2));
        }
        secp256k1_fe_normalize_var(&z);
        /* Test storage conversion and conditional moves. */
        secp256k1_fe_to_storage(&xs, &x);
        secp256k1_fe_to_storage(&ys, &y);
        secp256k1_fe_to_storage(&zs, &z);
        secp256k1_fe_storage_cmov(&zs, &xs, 0);
        secp256k1_fe_storage_cmov(&zs, &zs, 1);
        CHECK(memcmp(&xs, &zs, sizeof(xs)) != 0);
        secp256k1_fe_storage_cmov(&ys, &xs, 1);
        CHECK(memcmp(&xs, &ys, sizeof(xs)) == 0);
        secp256k1_fe_from_storage(&x, &xs);
        secp256k1_fe_from_storage(&y, &ys);
        secp256k1_fe_from_storage(&z, &zs);
        /* Test that mul_int, mul, and add agree. */
        secp256k1_fe_add(&y, &x);
        secp256k1_fe_add(&y, &x);
        z = x;
        secp256k1_fe_mul_int(&z, 3);
        CHECK(check_fe_equal(&y, &z));
        secp256k1_fe_add(&y, &x);
        secp256k1_fe_add(&z, &x);
        CHECK(check_fe_equal(&z, &y));
        z = x;
        secp256k1_fe_mul_int(&z, 5);
        secp256k1_fe_mul(&q, &x, &fe5);
        CHECK(check_fe_equal(&z, &q));
        secp256k1_fe_negate(&x, &x, 1);
        secp256k1_fe_add(&z, &x);
        secp256k1_fe_add(&q, &x);
        CHECK(check_fe_equal(&y, &z));
        CHECK(check_fe_equal(&q, &y));
    }
}

void run_field_inv(void) {
    secp256k1_fe x, xi, xii;
    int i;
    for (i = 0; i < 10*count; i++) {
        random_fe_non_zero(&x);
        secp256k1_fe_inv(&xi, &x);
        CHECK(check_fe_inverse(&x, &xi));
        secp256k1_fe_inv(&xii, &xi);
        CHECK(check_fe_equal(&x, &xii));
    }
}

void run_field_inv_var(void) {
    secp256k1_fe x, xi, xii;
    int i;
    for (i = 0; i < 10*count; i++) {
        random_fe_non_zero(&x);
        secp256k1_fe_inv_var(&xi, &x);
        CHECK(check_fe_inverse(&x, &xi));
        secp256k1_fe_inv_var(&xii, &xi);
        CHECK(check_fe_equal(&x, &xii));
    }
}

void run_field_inv_all_var(void) {
    secp256k1_fe x[16], xi[16], xii[16];
    int i;
    /* Check it's safe to call for 0 elements */
    secp256k1_fe_inv_all_var(xi, x, 0);
    for (i = 0; i < count; i++) {
        size_t j;
        size_t len = secp256k1_rand_int(15) + 1;
        for (j = 0; j < len; j++) {
            random_fe_non_zero(&x[j]);
        }
        secp256k1_fe_inv_all_var(xi, x, len);
        for (j = 0; j < len; j++) {
            CHECK(check_fe_inverse(&x[j], &xi[j]));
        }
        secp256k1_fe_inv_all_var(xii, xi, len);
        for (j = 0; j < len; j++) {
            CHECK(check_fe_equal(&x[j], &xii[j]));
        }
    }
}

void run_sqr(void) {
    secp256k1_fe x, s;

    {
        int i;
        secp256k1_fe_set_int(&x, 1);
        secp256k1_fe_negate(&x, &x, 1);

        for (i = 1; i <= 512; ++i) {
            secp256k1_fe_mul_int(&x, 2);
            secp256k1_fe_normalize(&x);
            secp256k1_fe_sqr(&s, &x);
        }
    }
}

void test_sqrt(const secp256k1_fe *a, const secp256k1_fe *k) {
    secp256k1_fe r1, r2;
    int v = secp256k1_fe_sqrt(&r1, a);
    CHECK((v == 0) == (k == NULL));

    if (k != NULL) {
        /* Check that the returned root is +/- the given known answer */
        secp256k1_fe_negate(&r2, &r1, 1);
        secp256k1_fe_add(&r1, k); secp256k1_fe_add(&r2, k);
        secp256k1_fe_normalize(&r1); secp256k1_fe_normalize(&r2);
        CHECK(secp256k1_fe_is_zero(&r1) || secp256k1_fe_is_zero(&r2));
    }
}

void run_sqrt(void) {
    secp256k1_fe ns, x, s, t;
    int i;

    /* Check sqrt(0) is 0 */
    secp256k1_fe_set_int(&x, 0);
    secp256k1_fe_sqr(&s, &x);
    test_sqrt(&s, &x);

    /* Check sqrt of small squares (and their negatives) */
    for (i = 1; i <= 100; i++) {
        secp256k1_fe_set_int(&x, i);
        secp256k1_fe_sqr(&s, &x);
        test_sqrt(&s, &x);
        secp256k1_fe_negate(&t, &s, 1);
        test_sqrt(&t, NULL);
    }

    /* Consistency checks for large random values */
    for (i = 0; i < 10; i++) {
        int j;
        random_fe_non_square(&ns);
        for (j = 0; j < count; j++) {
            random_fe(&x);
            secp256k1_fe_sqr(&s, &x);
            test_sqrt(&s, &x);
            secp256k1_fe_negate(&t, &s, 1);
            test_sqrt(&t, NULL);
            secp256k1_fe_mul(&t, &s, &ns);
            test_sqrt(&t, NULL);
        }
    }
}

/***** GROUP TESTS *****/

void ge_equals_ge(const secp256k1_ge *a, const secp256k1_ge *b) {
    CHECK(a->infinity == b->infinity);
    if (a->infinity) {
        return;
    }
    CHECK(secp256k1_fe_equal_var(&a->x, &b->x));
    CHECK(secp256k1_fe_equal_var(&a->y, &b->y));
}

/* This compares jacobian points including their Z, not just their geometric meaning. */
int gej_xyz_equals_gej(const secp256k1_gej *a, const secp256k1_gej *b) {
    secp256k1_gej a2;
    secp256k1_gej b2;
    int ret = 1;
    ret &= a->infinity == b->infinity;
    if (ret && !a->infinity) {
        a2 = *a;
        b2 = *b;
        secp256k1_fe_normalize(&a2.x);
        secp256k1_fe_normalize(&a2.y);
        secp256k1_fe_normalize(&a2.z);
        secp256k1_fe_normalize(&b2.x);
        secp256k1_fe_normalize(&b2.y);
        secp256k1_fe_normalize(&b2.z);
        ret &= secp256k1_fe_cmp_var(&a2.x, &b2.x) == 0;
        ret &= secp256k1_fe_cmp_var(&a2.y, &b2.y) == 0;
        ret &= secp256k1_fe_cmp_var(&a2.z, &b2.z) == 0;
    }
    return ret;
}

void ge_equals_gej(const secp256k1_ge *a, const secp256k1_gej *b) {
    secp256k1_fe z2s;
    secp256k1_fe u1, u2, s1, s2;
    CHECK(a->infinity == b->infinity);
    if (a->infinity) {
        return;
    }
    /* Check a.x * b.z^2 == b.x && a.y * b.z^3 == b.y, to avoid inverses. */
    secp256k1_fe_sqr(&z2s, &b->z);
    secp256k1_fe_mul(&u1, &a->x, &z2s);
    u2 = b->x; secp256k1_fe_normalize_weak(&u2);
    secp256k1_fe_mul(&s1, &a->y, &z2s); secp256k1_fe_mul(&s1, &s1, &b->z);
    s2 = b->y; secp256k1_fe_normalize_weak(&s2);
    CHECK(secp256k1_fe_equal_var(&u1, &u2));
    CHECK(secp256k1_fe_equal_var(&s1, &s2));
}

void test_ge(void) {
    int i, i1;
#ifdef USE_ENDOMORPHISM
    int runs = 6;
#else
    int runs = 4;
#endif
    /* Points: (infinity, p1, p1, -p1, -p1, p2, p2, -p2, -p2, p3, p3, -p3, -p3, p4, p4, -p4, -p4).
     * The second in each pair of identical points uses a random Z coordinate in the Jacobian form.
     * All magnitudes are randomized.
     * All 17*17 combinations of points are added to each other, using all applicable methods.
     *
     * When the endomorphism code is compiled in, p5 = lambda*p1 and p6 = lambda^2*p1 are added as well.
     */
    secp256k1_ge *ge = (secp256k1_ge *)checked_malloc(&ctx->error_callback, sizeof(secp256k1_ge) * (1 + 4 * runs));
    secp256k1_gej *gej = (secp256k1_gej *)checked_malloc(&ctx->error_callback, sizeof(secp256k1_gej) * (1 + 4 * runs));
    secp256k1_fe *zinv = (secp256k1_fe *)checked_malloc(&ctx->error_callback, sizeof(secp256k1_fe) * (1 + 4 * runs));
    secp256k1_fe zf;
    secp256k1_fe zfi2, zfi3;

    secp256k1_gej_set_infinity(&gej[0]);
    secp256k1_ge_clear(&ge[0]);
    secp256k1_ge_set_gej_var(&ge[0], &gej[0]);
    for (i = 0; i < runs; i++) {
        int j;
        secp256k1_ge g;
        random_group_element_test(&g);
#ifdef USE_ENDOMORPHISM
        if (i >= runs - 2) {
            secp256k1_ge_mul_lambda(&g, &ge[1]);
        }
        if (i >= runs - 1) {
            secp256k1_ge_mul_lambda(&g, &g);
        }
#endif
        ge[1 + 4 * i] = g;
        ge[2 + 4 * i] = g;
        secp256k1_ge_neg(&ge[3 + 4 * i], &g);
        secp256k1_ge_neg(&ge[4 + 4 * i], &g);
        secp256k1_gej_set_ge(&gej[1 + 4 * i], &ge[1 + 4 * i]);
        random_group_element_jacobian_test(&gej[2 + 4 * i], &ge[2 + 4 * i]);
        secp256k1_gej_set_ge(&gej[3 + 4 * i], &ge[3 + 4 * i]);
        random_group_element_jacobian_test(&gej[4 + 4 * i], &ge[4 + 4 * i]);
        for (j = 0; j < 4; j++) {
            random_field_element_magnitude(&ge[1 + j + 4 * i].x);
            random_field_element_magnitude(&ge[1 + j + 4 * i].y);
            random_field_element_magnitude(&gej[1 + j + 4 * i].x);
            random_field_element_magnitude(&gej[1 + j + 4 * i].y);
            random_field_element_magnitude(&gej[1 + j + 4 * i].z);
        }
    }

    /* Compute z inverses. */
    {
        secp256k1_fe *zs = checked_malloc(&ctx->error_callback, sizeof(secp256k1_fe) * (1 + 4 * runs));
        for (i = 0; i < 4 * runs + 1; i++) {
            if (i == 0) {
                /* The point at infinity does not have a meaningful z inverse. Any should do. */
                do {
                    random_field_element_test(&zs[i]);
                } while(secp256k1_fe_is_zero(&zs[i]));
            } else {
                zs[i] = gej[i].z;
            }
        }
        secp256k1_fe_inv_all_var(zinv, zs, 4 * runs + 1);
        free(zs);
    }

    /* Generate random zf, and zfi2 = 1/zf^2, zfi3 = 1/zf^3 */
    do {
        random_field_element_test(&zf);
    } while(secp256k1_fe_is_zero(&zf));
    random_field_element_magnitude(&zf);
    secp256k1_fe_inv_var(&zfi3, &zf);
    secp256k1_fe_sqr(&zfi2, &zfi3);
    secp256k1_fe_mul(&zfi3, &zfi3, &zfi2);

    for (i1 = 0; i1 < 1 + 4 * runs; i1++) {
        int i2;
        for (i2 = 0; i2 < 1 + 4 * runs; i2++) {
            /* Compute reference result using gej + gej (var). */
            secp256k1_gej refj, resj;
            secp256k1_ge ref;
            secp256k1_fe zr;
            secp256k1_gej_add_var(&refj, &gej[i1], &gej[i2], secp256k1_gej_is_infinity(&gej[i1]) ? NULL : &zr);
            /* Check Z ratio. */
            if (!secp256k1_gej_is_infinity(&gej[i1]) && !secp256k1_gej_is_infinity(&refj)) {
                secp256k1_fe zrz; secp256k1_fe_mul(&zrz, &zr, &gej[i1].z);
                CHECK(secp256k1_fe_equal_var(&zrz, &refj.z));
            }
            secp256k1_ge_set_gej_var(&ref, &refj);

            /* Test gej + ge with Z ratio result (var). */
            secp256k1_gej_add_ge_var(&resj, &gej[i1], &ge[i2], secp256k1_gej_is_infinity(&gej[i1]) ? NULL : &zr);
            ge_equals_gej(&ref, &resj);
            if (!secp256k1_gej_is_infinity(&gej[i1]) && !secp256k1_gej_is_infinity(&resj)) {
                secp256k1_fe zrz; secp256k1_fe_mul(&zrz, &zr, &gej[i1].z);
                CHECK(secp256k1_fe_equal_var(&zrz, &resj.z));
            }

            /* Test gej + ge (var, with additional Z factor). */
            {
                secp256k1_ge ge2_zfi = ge[i2]; /* the second term with x and y rescaled for z = 1/zf */
                secp256k1_fe_mul(&ge2_zfi.x, &ge2_zfi.x, &zfi2);
                secp256k1_fe_mul(&ge2_zfi.y, &ge2_zfi.y, &zfi3);
                random_field_element_magnitude(&ge2_zfi.x);
                random_field_element_magnitude(&ge2_zfi.y);
                secp256k1_gej_add_zinv_var(&resj, &gej[i1], &ge2_zfi, &zf);
                ge_equals_gej(&ref, &resj);
            }

            /* Test gej + ge (const). */
            if (i2 != 0) {
                /* secp256k1_gej_add_ge does not support its second argument being infinity. */
                secp256k1_gej_add_ge(&resj, &gej[i1], &ge[i2]);
                ge_equals_gej(&ref, &resj);
            }

            /* Test doubling (var). */
            if ((i1 == 0 && i2 == 0) || ((i1 + 3)/4 == (i2 + 3)/4 && ((i1 + 3)%4)/2 == ((i2 + 3)%4)/2)) {
                secp256k1_fe zr2;
                /* Normal doubling with Z ratio result. */
                secp256k1_gej_double_var(&resj, &gej[i1], &zr2);
                ge_equals_gej(&ref, &resj);
                /* Check Z ratio. */
                secp256k1_fe_mul(&zr2, &zr2, &gej[i1].z);
                CHECK(secp256k1_fe_equal_var(&zr2, &resj.z));
                /* Normal doubling. */
                secp256k1_gej_double_var(&resj, &gej[i2], NULL);
                ge_equals_gej(&ref, &resj);
            }

            /* Test adding opposites. */
            if ((i1 == 0 && i2 == 0) || ((i1 + 3)/4 == (i2 + 3)/4 && ((i1 + 3)%4)/2 != ((i2 + 3)%4)/2)) {
                CHECK(secp256k1_ge_is_infinity(&ref));
            }

            /* Test adding infinity. */
            if (i1 == 0) {
                CHECK(secp256k1_ge_is_infinity(&ge[i1]));
                CHECK(secp256k1_gej_is_infinity(&gej[i1]));
                ge_equals_gej(&ref, &gej[i2]);
            }
            if (i2 == 0) {
                CHECK(secp256k1_ge_is_infinity(&ge[i2]));
                CHECK(secp256k1_gej_is_infinity(&gej[i2]));
                ge_equals_gej(&ref, &gej[i1]);
            }
        }
    }

    /* Test adding all points together in random order equals infinity. */
    {
        secp256k1_gej sum = SECP256K1_GEJ_CONST_INFINITY;
        secp256k1_gej *gej_shuffled = (secp256k1_gej *)checked_malloc(&ctx->error_callback, (4 * runs + 1) * sizeof(secp256k1_gej));
        for (i = 0; i < 4 * runs + 1; i++) {
            gej_shuffled[i] = gej[i];
        }
        for (i = 0; i < 4 * runs + 1; i++) {
            int swap = i + secp256k1_rand_int(4 * runs + 1 - i);
            if (swap != i) {
                secp256k1_gej t = gej_shuffled[i];
                gej_shuffled[i] = gej_shuffled[swap];
                gej_shuffled[swap] = t;
            }
        }
        for (i = 0; i < 4 * runs + 1; i++) {
            secp256k1_gej_add_var(&sum, &sum, &gej_shuffled[i], NULL);
        }
        CHECK(secp256k1_gej_is_infinity(&sum));
        free(gej_shuffled);
    }

    /* Test batch gej -> ge conversion with and without known z ratios. */
    {
        secp256k1_fe *zr = (secp256k1_fe *)checked_malloc(&ctx->error_callback, (4 * runs + 1) * sizeof(secp256k1_fe));
        secp256k1_ge *ge_set_table = (secp256k1_ge *)checked_malloc(&ctx->error_callback, (4 * runs + 1) * sizeof(secp256k1_ge));
        secp256k1_ge *ge_set_all = (secp256k1_ge *)checked_malloc(&ctx->error_callback, (4 * runs + 1) * sizeof(secp256k1_ge));
        for (i = 0; i < 4 * runs + 1; i++) {
            /* Compute gej[i + 1].z / gez[i].z (with gej[n].z taken to be 1). */
            if (i < 4 * runs) {
                secp256k1_fe_mul(&zr[i + 1], &zinv[i], &gej[i + 1].z);
            }
        }
        secp256k1_ge_set_table_gej_var(ge_set_table, gej, zr, 4 * runs + 1);
        secp256k1_ge_set_all_gej_var(ge_set_all, gej, 4 * runs + 1, &ctx->error_callback);
        for (i = 0; i < 4 * runs + 1; i++) {
            secp256k1_fe s;
            random_fe_non_zero(&s);
            secp256k1_gej_rescale(&gej[i], &s);
            ge_equals_gej(&ge_set_table[i], &gej[i]);
            ge_equals_gej(&ge_set_all[i], &gej[i]);
        }
        free(ge_set_table);
        free(ge_set_all);
        free(zr);
    }

    free(ge);
    free(gej);
    free(zinv);
}

void test_add_neg_y_diff_x(void) {
    /* The point of this test is to check that we can add two points
     * whose y-coordinates are negatives of each other but whose x
     * coordinates differ. If the x-coordinates were the same, these
     * points would be negatives of each other and their sum is
     * infinity. This is cool because it "covers up" any degeneracy
     * in the addition algorithm that would cause the xy coordinates
     * of the sum to be wrong (since infinity has no xy coordinates).
     * HOWEVER, if the x-coordinates are different, infinity is the
     * wrong answer, and such degeneracies are exposed. This is the
     * root of https://github.com/bitcoin-core/secp256k1/issues/257
     * which this test is a regression test for.
     *
     * These points were generated in sage as
     * # secp256k1 params
     * F = FiniteField (0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F)
     * C = EllipticCurve ([F (0), F (7)])
     * G = C.lift_x(0x79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798)
     * N = FiniteField(G.order())
     *
     * # endomorphism values (lambda is 1^{1/3} in N, beta is 1^{1/3} in F)
     * x = polygen(N)
     * lam  = (1 - x^3).roots()[1][0]
     *
     * # random "bad pair"
     * P = C.random_element()
     * Q = -int(lam) * P
     * print "    P: %x %x" % P.xy()
     * print "    Q: %x %x" % Q.xy()
     * print "P + Q: %x %x" % (P + Q).xy()
     */
    secp256k1_gej aj = SECP256K1_GEJ_CONST(
        0x8d24cd95, 0x0a355af1, 0x3c543505, 0x44238d30,
        0x0643d79f, 0x05a59614, 0x2f8ec030, 0xd58977cb,
        0x001e337a, 0x38093dcd, 0x6c0f386d, 0x0b1293a8,
        0x4d72c879, 0xd7681924, 0x44e6d2f3, 0x9190117d
    );
    secp256k1_gej bj = SECP256K1_GEJ_CONST(
        0xc7b74206, 0x1f788cd9, 0xabd0937d, 0x164a0d86,
        0x95f6ff75, 0xf19a4ce9, 0xd013bd7b, 0xbf92d2a7,
        0xffe1cc85, 0xc7f6c232, 0x93f0c792, 0xf4ed6c57,
        0xb28d3786, 0x2897e6db, 0xbb192d0b, 0x6e6feab2
    );
    secp256k1_gej sumj = SECP256K1_GEJ_CONST(
        0x671a63c0, 0x3efdad4c, 0x389a7798, 0x24356027,
        0xb3d69010, 0x278625c3, 0x5c86d390, 0x184a8f7a,
        0x5f6409c2, 0x2ce01f2b, 0x511fd375, 0x25071d08,
        0xda651801, 0x70e95caf, 0x8f0d893c, 0xbed8fbbe
    );
    secp256k1_ge b;
    secp256k1_gej resj;
    secp256k1_ge res;
    secp256k1_ge_set_gej(&b, &bj);

    secp256k1_gej_add_var(&resj, &aj, &bj, NULL);
    secp256k1_ge_set_gej(&res, &resj);
    ge_equals_gej(&res, &sumj);

    secp256k1_gej_add_ge(&resj, &aj, &b);
    secp256k1_ge_set_gej(&res, &resj);
    ge_equals_gej(&res, &sumj);

    secp256k1_gej_add_ge_var(&resj, &aj, &b, NULL);
    secp256k1_ge_set_gej(&res, &resj);
    ge_equals_gej(&res, &sumj);
}

void run_ge(void) {
    int i;
    for (i = 0; i < count * 32; i++) {
        test_ge();
    }
    test_add_neg_y_diff_x();
}

void test_ec_combine(void) {
    secp256k1_scalar sum = SECP256K1_SCALAR_CONST(0, 0, 0, 0, 0, 0, 0, 0);
    secp256k1_pubkey data[6];
    const secp256k1_pubkey* d[6];
    secp256k1_pubkey sd;
    secp256k1_pubkey sd2;
    secp256k1_gej Qj;
    secp256k1_ge Q;
    int i;
    for (i = 1; i <= 6; i++) {
        secp256k1_scalar s;
        random_scalar_order_test(&s);
        secp256k1_scalar_add(&sum, &sum, &s);
        secp256k1_ecmult_gen(&ctx->ecmult_gen_ctx, &Qj, &s);
        secp256k1_ge_set_gej(&Q, &Qj);
        secp256k1_pubkey_save(&data[i - 1], &Q);
        d[i - 1] = &data[i - 1];
        secp256k1_ecmult_gen(&ctx->ecmult_gen_ctx, &Qj, &sum);
        secp256k1_ge_set_gej(&Q, &Qj);
        secp256k1_pubkey_save(&sd, &Q);
        CHECK(secp256k1_ec_pubkey_combine(ctx, &sd2, d, i) == 1);
        CHECK(memcmp(&sd, &sd2, sizeof(sd)) == 0);
    }
}

void run_ec_combine(void) {
    int i;
    for (i = 0; i < count * 8; i++) {
         test_ec_combine();
    }
}

void test_group_decompress(const secp256k1_fe* x) {
    /* The input itself, normalized. */
    secp256k1_fe fex = *x;
    secp256k1_fe fez;
    /* Results of set_xquad_var, set_xo_var(..., 0), set_xo_var(..., 1). */
    secp256k1_ge ge_quad, ge_even, ge_odd;
    secp256k1_gej gej_quad;
    /* Return values of the above calls. */
    int res_quad, res_even, res_odd;

    secp256k1_fe_normalize_var(&fex);

    res_quad = secp256k1_ge_set_xquad(&ge_quad, &fex);
    res_even = secp256k1_ge_set_xo_var(&ge_even, &fex, 0);
    res_odd = secp256k1_ge_set_xo_var(&ge_odd, &fex, 1);

    CHECK(res_quad == res_even);
    CHECK(res_quad == res_odd);

    if (res_quad) {
        secp256k1_fe_normalize_var(&ge_quad.x);
        secp256k1_fe_normalize_var(&ge_odd.x);
        secp256k1_fe_normalize_var(&ge_even.x);
        secp256k1_fe_normalize_var(&ge_quad.y);
        secp256k1_fe_normalize_var(&ge_odd.y);
        secp256k1_fe_normalize_var(&ge_even.y);

        /* No infinity allowed. */
        CHECK(!ge_quad.infinity);
        CHECK(!ge_even.infinity);
        CHECK(!ge_odd.infinity);

        /* Check that the x coordinates check out. */
        CHECK(secp256k1_fe_equal_var(&ge_quad.x, x));
        CHECK(secp256k1_fe_equal_var(&ge_even.x, x));
        CHECK(secp256k1_fe_equal_var(&ge_odd.x, x));

        /* Check that the Y coordinate result in ge_quad is a square. */
        CHECK(secp256k1_fe_is_quad_var(&ge_quad.y));

        /* Check odd/even Y in ge_odd, ge_even. */
        CHECK(secp256k1_fe_is_odd(&ge_odd.y));
        CHECK(!secp256k1_fe_is_odd(&ge_even.y));

        /* Check secp256k1_gej_has_quad_y_var. */
        secp256k1_gej_set_ge(&gej_quad, &ge_quad);
        CHECK(secp256k1_gej_has_quad_y_var(&gej_quad));
        do {
            random_fe_test(&fez);
        } while (secp256k1_fe_is_zero(&fez));
        secp256k1_gej_rescale(&gej_quad, &fez);
        CHECK(secp256k1_gej_has_quad_y_var(&gej_quad));
        secp256k1_gej_neg(&gej_quad, &gej_quad);
        CHECK(!secp256k1_gej_has_quad_y_var(&gej_quad));
        do {
            random_fe_test(&fez);
        } while (secp256k1_fe_is_zero(&fez));
        secp256k1_gej_rescale(&gej_quad, &fez);
        CHECK(!secp256k1_gej_has_quad_y_var(&gej_quad));
        secp256k1_gej_neg(&gej_quad, &gej_quad);
        CHECK(secp256k1_gej_has_quad_y_var(&gej_quad));
    }
}

void run_group_decompress(void) {
    int i;
    for (i = 0; i < count * 4; i++) {
        secp256k1_fe fe;
        random_fe_test(&fe);
        test_group_decompress(&fe);
    }
}

/***** ECMULT TESTS *****/

void run_ecmult_chain(void) {
    /* random starting point A (on the curve) */
    secp256k1_gej a = SECP256K1_GEJ_CONST(
        0x8b30bbe9, 0xae2a9906, 0x96b22f67, 0x0709dff3,
        0x727fd8bc, 0x04d3362c, 0x6c7bf458, 0xe2846004,
        0xa357ae91, 0x5c4a6528, 0x1309edf2, 0x0504740f,
        0x0eb33439, 0x90216b4f, 0x81063cb6, 0x5f2f7e0f
    );
    /* two random initial factors xn and gn */
    secp256k1_scalar xn = SECP256K1_SCALAR_CONST(
        0x84cc5452, 0xf7fde1ed, 0xb4d38a8c, 0xe9b1b84c,
        0xcef31f14, 0x6e569be9, 0x705d357a, 0x42985407
    );
    secp256k1_scalar gn = SECP256K1_SCALAR_CONST(
        0xa1e58d22, 0x553dcd42, 0xb2398062, 0x5d4c57a9,
        0x6e9323d4, 0x2b3152e5, 0xca2c3990, 0xedc7c9de
    );
    /* two small multipliers to be applied to xn and gn in every iteration: */
    static const secp256k1_scalar xf = SECP256K1_SCALAR_CONST(0, 0, 0, 0, 0, 0, 0, 0x1337);
    static const secp256k1_scalar gf = SECP256K1_SCALAR_CONST(0, 0, 0, 0, 0, 0, 0, 0x7113);
    /* accumulators with the resulting coefficients to A and G */
    secp256k1_scalar ae = SECP256K1_SCALAR_CONST(0, 0, 0, 0, 0, 0, 0, 1);
    secp256k1_scalar ge = SECP256K1_SCALAR_CONST(0, 0, 0, 0, 0, 0, 0, 0);
    /* actual points */
    secp256k1_gej x;
    secp256k1_gej x2;
    int i;

    /* the point being computed */
    x = a;
    for (i = 0; i < 200*count; i++) {
        /* in each iteration, compute X = xn*X + gn*G; */
        secp256k1_ecmult(&ctx->ecmult_ctx, &x, &x, &xn, &gn);
        /* also compute ae and ge: the actual accumulated factors for A and G */
        /* if X was (ae*A+ge*G), xn*X + gn*G results in (xn*ae*A + (xn*ge+gn)*G) */
        secp256k1_scalar_mul(&ae, &ae, &xn);
        secp256k1_scalar_mul(&ge, &ge, &xn);
        secp256k1_scalar_add(&ge, &ge, &gn);
        /* modify xn and gn */
        secp256k1_scalar_mul(&xn, &xn, &xf);
        secp256k1_scalar_mul(&gn, &gn, &gf);

        /* verify */
        if (i == 19999) {
            /* expected result after 19999 iterations */
            secp256k1_gej rp = SECP256K1_GEJ_CONST(
                0xD6E96687, 0xF9B10D09, 0x2A6F3543, 0x9D86CEBE,
                0xA4535D0D, 0x409F5358, 0x6440BD74, 0xB933E830,
                0xB95CBCA2, 0xC77DA786, 0x539BE8FD, 0x53354D2D,
                0x3B4F566A, 0xE6580454, 0x07ED6015, 0xEE1B2A88
            );

            secp256k1_gej_neg(&rp, &rp);
            secp256k1_gej_add_var(&rp, &rp, &x, NULL);
            CHECK(secp256k1_gej_is_infinity(&rp));
        }
    }
    /* redo the computation, but directly with the resulting ae and ge coefficients: */
    secp256k1_ecmult(&ctx->ecmult_ctx, &x2, &a, &ae, &ge);
    secp256k1_gej_neg(&x2, &x2);
    secp256k1_gej_add_var(&x2, &x2, &x, NULL);
    CHECK(secp256k1_gej_is_infinity(&x2));
}

void test_point_times_order(const secp256k1_gej *point) {
    /* X * (point + G) + (order-X) * (pointer + G) = 0 */
    secp256k1_scalar x;
    secp256k1_scalar nx;
    secp256k1_scalar zero = SECP256K1_SCALAR_CONST(0, 0, 0, 0, 0, 0, 0, 0);
    secp256k1_scalar one = SECP256K1_SCALAR_CONST(0, 0, 0, 0, 0, 0, 0, 1);
    secp256k1_gej res1, res2;
    secp256k1_ge res3;
    unsigned char pub[65];
    size_t psize = 65;
    random_scalar_order_test(&x);
    secp256k1_scalar_negate(&nx, &x);
    secp256k1_ecmult(&ctx->ecmult_ctx, &res1, point, &x, &x); /* calc res1 = x * point + x * G; */
    secp256k1_ecmult(&ctx->ecmult_ctx, &res2, point, &nx, &nx); /* calc res2 = (order - x) * point + (order - x) * G; */
    secp256k1_gej_add_var(&res1, &res1, &res2, NULL);
    CHECK(secp256k1_gej_is_infinity(&res1));
    CHECK(secp256k1_gej_is_valid_var(&res1) == 0);
    secp256k1_ge_set_gej(&res3, &res1);
    CHECK(secp256k1_ge_is_infinity(&res3));
    CHECK(secp256k1_ge_is_valid_var(&res3) == 0);
    CHECK(secp256k1_eckey_pubkey_serialize(&res3, pub, &psize, 0) == 0);
    psize = 65;
    CHECK(secp256k1_eckey_pubkey_serialize(&res3, pub, &psize, 1) == 0);
    /* check zero/one edge cases */
    secp256k1_ecmult(&ctx->ecmult_ctx, &res1, point, &zero, &zero);
    secp256k1_ge_set_gej(&res3, &res1);
    CHECK(secp256k1_ge_is_infinity(&res3));
    secp256k1_ecmult(&ctx->ecmult_ctx, &res1, point, &one, &zero);
    secp256k1_ge_set_gej(&res3, &res1);
    ge_equals_gej(&res3, point);
    secp256k1_ecmult(&ctx->ecmult_ctx, &res1, point, &zero, &one);
    secp256k1_ge_set_gej(&res3, &res1);
    ge_equals_ge(&res3, &secp256k1_ge_const_g);
}

void run_point_times_order(void) {
    int i;
    secp256k1_fe x = SECP256K1_FE_CONST(0, 0, 0, 0, 0, 0, 0, 2);
    static const secp256k1_fe xr = SECP256K1_FE_CONST(
        0x7603CB59, 0xB0EF6C63, 0xFE608479, 0x2A0C378C,
        0xDB3233A8, 0x0F8A9A09, 0xA877DEAD, 0x31B38C45
    );
    for (i = 0; i < 500; i++) {
        secp256k1_ge p;
        if (secp256k1_ge_set_xo_var(&p, &x, 1)) {
            secp256k1_gej j;
            CHECK(secp256k1_ge_is_valid_var(&p));
            secp256k1_gej_set_ge(&j, &p);
            CHECK(secp256k1_gej_is_valid_var(&j));
            test_point_times_order(&j);
        }
        secp256k1_fe_sqr(&x, &x);
    }
    secp256k1_fe_normalize_var(&x);
    CHECK(secp256k1_fe_equal_var(&x, &xr));
}

void ecmult_const_random_mult(void) {
    /* random starting point A (on the curve) */
    secp256k1_ge a = SECP256K1_GE_CONST(
        0x6d986544, 0x57ff52b8, 0xcf1b8126, 0x5b802a5b,
        0xa97f9263, 0xb1e88044, 0x93351325, 0x91bc450a,
        0x535c59f7, 0x325e5d2b, 0xc391fbe8, 0x3c12787c,
        0x337e4a98, 0xe82a9011, 0x0123ba37, 0xdd769c7d
    );
    /* random initial factor xn */
    secp256k1_scalar xn = SECP256K1_SCALAR_CONST(
        0x649d4f77, 0xc4242df7, 0x7f2079c9, 0x14530327,
        0xa31b876a, 0xd2d8ce2a, 0x2236d5c6, 0xd7b2029b
    );
    /* expected xn * A (from sage) */
    secp256k1_ge expected_b = SECP256K1_GE_CONST(
        0x23773684, 0x4d209dc7, 0x098a786f, 0x20d06fcd,
        0x070a38bf, 0xc11ac651, 0x03004319, 0x1e2a8786,
        0xed8c3b8e, 0xc06dd57b, 0xd06ea66e, 0x45492b0f,
        0xb84e4e1b, 0xfb77e21f, 0x96baae2a, 0x63dec956
    );
    secp256k1_gej b;
    secp256k1_ecmult_const(&b, &a, &xn,256);

    CHECK(secp256k1_ge_is_valid_var(&a));
    ge_equals_gej(&expected_b, &b);
}

void ecmult_const_commutativity(void) {
    secp256k1_scalar a;
    secp256k1_scalar b;
    secp256k1_gej res1;
    secp256k1_gej res2;
    secp256k1_ge mid1;
    secp256k1_ge mid2;
    random_scalar_order_test(&a);
    random_scalar_order_test(&b);

    secp256k1_ecmult_const(&res1, &secp256k1_ge_const_g, &a,256);
    secp256k1_ecmult_const(&res2, &secp256k1_ge_const_g, &b,256);
    secp256k1_ge_set_gej(&mid1, &res1);
    secp256k1_ge_set_gej(&mid2, &res2);
    secp256k1_ecmult_const(&res1, &mid1, &b,256);
    secp256k1_ecmult_const(&res2, &mid2, &a,256);
    secp256k1_ge_set_gej(&mid1, &res1);
    secp256k1_ge_set_gej(&mid2, &res2);
    ge_equals_ge(&mid1, &mid2);
}

void ecmult_const_mult_zero_one(void) {
    secp256k1_scalar zero = SECP256K1_SCALAR_CONST(0, 0, 0, 0, 0, 0, 0, 0);
    secp256k1_scalar one = SECP256K1_SCALAR_CONST(0, 0, 0, 0, 0, 0, 0, 1);
    secp256k1_scalar negone;
    secp256k1_gej res1;
    secp256k1_ge res2;
    secp256k1_ge point;
    secp256k1_scalar_negate(&negone, &one);

    random_group_element_test(&point);
    secp256k1_ecmult_const(&res1, &point, &zero,256);
    secp256k1_ge_set_gej(&res2, &res1);
    CHECK(secp256k1_ge_is_infinity(&res2));
    secp256k1_ecmult_const(&res1, &point, &one,256);
    secp256k1_ge_set_gej(&res2, &res1);
    ge_equals_ge(&res2, &point);
    secp256k1_ecmult_const(&res1, &point, &negone,256);
    secp256k1_gej_neg(&res1, &res1);
    secp256k1_ge_set_gej(&res2, &res1);
    ge_equals_ge(&res2, &point);
}

void ecmult_const_chain_multiply(void) {
    /* Check known result (randomly generated test problem from sage) */
    const secp256k1_scalar scalar = SECP256K1_SCALAR_CONST(
        0x4968d524, 0x2abf9b7a, 0x466abbcf, 0x34b11b6d,
        0xcd83d307, 0x827bed62, 0x05fad0ce, 0x18fae63b
    );
    const secp256k1_gej expected_point = SECP256K1_GEJ_CONST(
        0x5494c15d, 0x32099706, 0xc2395f94, 0x348745fd,
        0x757ce30e, 0x4e8c90fb, 0xa2bad184, 0xf883c69f,
        0x5d195d20, 0xe191bf7f, 0x1be3e55f, 0x56a80196,
        0x6071ad01, 0xf1462f66, 0xc997fa94, 0xdb858435
    );
    secp256k1_gej point;
    secp256k1_ge res;
    int i;

    secp256k1_gej_set_ge(&point, &secp256k1_ge_const_g);
    for (i = 0; i < 100; ++i) {
        secp256k1_ge tmp;
        secp256k1_ge_set_gej(&tmp, &point);
        secp256k1_ecmult_const(&point, &tmp, &scalar,256);
    }
    secp256k1_ge_set_gej(&res, &point);
    ge_equals_gej(&res, &expected_point);
}

void run_ecmult_const_tests(void) {
    ecmult_const_mult_zero_one();
    ecmult_const_random_mult();
    ecmult_const_commutativity();
    ecmult_const_chain_multiply();
}

void test_wnaf(const secp256k1_scalar *number, int w) {
    secp256k1_scalar x, two, t;
    int wnaf[256];
    int zeroes = -1;
    int i;
    int bits;
    secp256k1_scalar_set_int(&x, 0);
    secp256k1_scalar_set_int(&two, 2);
    bits = secp256k1_ecmult_wnaf(wnaf, 256, number, w);
    CHECK(bits <= 256);
    for (i = bits-1; i >= 0; i--) {
        int v = wnaf[i];
        secp256k1_scalar_mul(&x, &x, &two);
        if (v) {
            CHECK(zeroes == -1 || zeroes >= w-1); /* check that distance between non-zero elements is at least w-1 */
            zeroes=0;
            CHECK((v & 1) == 1); /* check non-zero elements are odd */
            CHECK(v <= (1 << (w-1)) - 1); /* check range below */
            CHECK(v >= -(1 << (w-1)) - 1); /* check range above */
        } else {
            CHECK(zeroes != -1); /* check that no unnecessary zero padding exists */
            zeroes++;
        }
        if (v >= 0) {
            secp256k1_scalar_set_int(&t, v);
        } else {
            secp256k1_scalar_set_int(&t, -v);
            secp256k1_scalar_negate(&t, &t);
        }
        secp256k1_scalar_add(&x, &x, &t);
    }
    CHECK(secp256k1_scalar_eq(&x, number)); /* check that wnaf represents number */
}

void test_constant_wnaf_negate(const secp256k1_scalar *number) {
    secp256k1_scalar neg1 = *number;
    secp256k1_scalar neg2 = *number;
    int sign1 = 1;
    int sign2 = 1;

    if (!secp256k1_scalar_get_bits(&neg1, 0, 1)) {
        secp256k1_scalar_negate(&neg1, &neg1);
        sign1 = -1;
    }
    sign2 = secp256k1_scalar_cond_negate(&neg2, secp256k1_scalar_is_even(&neg2));
    CHECK(sign1 == sign2);
    CHECK(secp256k1_scalar_eq(&neg1, &neg2));
}

void test_constant_wnaf(const secp256k1_scalar *number, int w) {
    secp256k1_scalar x, shift;
    int wnaf[256] = {0};
    int i;
    int skew;
    secp256k1_scalar num = *number;

    secp256k1_scalar_set_int(&x, 0);
    secp256k1_scalar_set_int(&shift, 1 << w);
    /* With USE_ENDOMORPHISM on we only consider 128-bit numbers */
#ifdef USE_ENDOMORPHISM
    for (i = 0; i < 16; ++i) {
        secp256k1_scalar_shr_int(&num, 8);
    }
#endif
    skew = secp256k1_wnaf_const(wnaf, num, w);

    for (i = WNAF_SIZE(w); i >= 0; --i) {
        secp256k1_scalar t;
        int v = wnaf[i];
        CHECK(v != 0); /* check nonzero */
        CHECK(v & 1);  /* check parity */
        CHECK(v > -(1 << w)); /* check range above */
        CHECK(v < (1 << w));  /* check range below */

        secp256k1_scalar_mul(&x, &x, &shift);
        if (v >= 0) {
            secp256k1_scalar_set_int(&t, v);
        } else {
            secp256k1_scalar_set_int(&t, -v);
            secp256k1_scalar_negate(&t, &t);
        }
        secp256k1_scalar_add(&x, &x, &t);
    }
    /* Skew num because when encoding numbers as odd we use an offset */
    secp256k1_scalar_cadd_bit(&num, skew == 2, 1);
    CHECK(secp256k1_scalar_eq(&x, &num));
}

void run_wnaf(void) {
    int i;
    secp256k1_scalar n = {{0}};

    /* Sanity check: 1 and 2 are the smallest odd and even numbers and should
     *               have easier-to-diagnose failure modes  */
    n.d[0] = 1;
    test_constant_wnaf(&n, 4);
    n.d[0] = 2;
    test_constant_wnaf(&n, 4);
    /* Random tests */
    for (i = 0; i < count; i++) {
        random_scalar_order(&n);
        test_wnaf(&n, 4+(i%10));
        test_constant_wnaf_negate(&n);
        test_constant_wnaf(&n, 4 + (i % 10));
    }
    secp256k1_scalar_set_int(&n, 0);
    CHECK(secp256k1_scalar_cond_negate(&n, 1) == -1);
    CHECK(secp256k1_scalar_is_zero(&n));
    CHECK(secp256k1_scalar_cond_negate(&n, 0) == 1);
    CHECK(secp256k1_scalar_is_zero(&n));
}

void test_ecmult_constants(void) {
    /* Test ecmult_gen() for [0..36) and [order-36..0). */
    secp256k1_scalar x;
    secp256k1_gej r;
    secp256k1_ge ng;
    int i;
    int j;
    secp256k1_ge_neg(&ng, &secp256k1_ge