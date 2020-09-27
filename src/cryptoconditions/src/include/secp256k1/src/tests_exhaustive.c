/***********************************************************************
 * Copyright (c) 2016 Andrew Poelstra                                 *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#if defined HAVE_CONFIG_H
#include "libsecp256k1-config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <time.h>

#undef USE_ECMULT_STATIC_PRECOMPUTATION

#ifndef EXHAUSTIVE_TEST_ORDER
/* see group_impl.h for allowable values */
#define EXHAUSTIVE_TEST_ORDER 13
#define EXHAUSTIVE_TEST_LAMBDA 9   /* cube root of 1 mod 13 */
#endif

#include "include/secp256k1.h"
#include "group.h"
#include "secp256k1.c"
#include "testrand_impl.h"

#ifdef ENABLE_MODULE_RECOVERY
#include "src/modules/recovery/main_impl.h"
#include "include/secp256k1_recovery.h"
#endif

/** stolen from tests.c */
void ge_equals_ge(const secp256k1_ge *a, const secp256k1_ge *b) {
    CHECK(a->infinity == b->infinity);
    if (a->infinity) {
        return;
    }
    CHECK(secp256k1_fe_equal_var(&a->x, &b->x));
    CHECK(secp256k1_fe_equal_var(&a->y, &b->y));
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

void random_fe(secp256k1_fe *x) {
    unsigned char bin[32];
    do {
        secp256k1_rand256(bin);
        if (secp256k1_fe_set_b32(x, bin)) {
            return;
        }
    } while(1);
}
/** END stolen from tests.c */

int secp256k1_nonce_function_smallint(unsigned char *nonce32, const unsigned char *msg32,
                                      const unsigned char *key