/**********************************************************************
 * Copyright (c) 2014-2015 Pieter Wuille                              *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/
#include <stdio.h>

#include "include/secp256k1.h"

#include "util.h"
#include "hash_impl.h"
#include "num_impl.h"
#include "field_impl.h"
#include "group_impl.h"
#include "scalar_impl.h"
#include "ecmult_const_impl.h"
#include "ecmult_impl.h"
#include "bench.h"
#include "secp256k1.c"

typedef struct {
    secp256k1_scalar scalar_x, scalar_y;
    secp256k1_fe fe_x, fe_y;
    secp256k1_ge ge_x, ge_y;
    secp256k1_gej gej_x, gej_y;
    unsigned char data[64];
    int wnaf[256];
} bench_inv;

void bench_setup(void* arg) {
    bench_inv *data = (bench_inv*)arg;

    static const unsigned char init_x[32] = {
        0x02, 0x03, 0x05, 0x07, 0x0b, 0x0d, 0x11, 0x13,
        0x17, 0x1d, 0x1f, 0x25, 0x29, 0x2b, 0x2f, 0x35,
        0x3b, 0x3d, 0x43, 0x47, 0x49, 0x4f, 0x53, 0x59,
        0x61, 0x65, 0x67, 0x6b, 0x6d, 0x71, 0x7f, 0x83
    };

    static const unsigned char init_y[32] = {
        0x82, 0x83, 0x85, 0x87, 0x8b, 0x8d, 0x81, 0x83,
        0x97, 0xad, 0xaf, 0xb5, 0xb9, 0xbb, 0xbf, 0xc5,
        0xdb, 0xdd, 0xe3, 0xe7, 0xe9, 0xef, 0xf3, 0xf9,
        0x11, 0x15, 0x17, 0x1b, 0x1d, 0xb1, 0xbf, 0xd3
    };

    secp256k1_scalar_set_b32(&data->scalar_x, init_x, NULL);
    secp256k1_scalar_set_b32(&data->scalar_y, init_y, NULL);
    secp256k1_fe_set_b32(&data->fe_x, init_x);
    secp256k1_fe_set_b32(&data->fe_y, init_y);
    CHECK(secp256k1_ge_set_xo_var(&data->ge_x, &data->fe_x, 0));
    CHECK(secp256k1_ge_set_xo_var(&data->ge_y, &data->fe_y, 1));
    secp256k1_gej_set_ge(&data->gej_x, &data->ge_x);
    secp256k1_gej_set_ge(&data->gej_y, &data->ge_y);
    memcpy(data->data, init_x, 32);
    memcpy(data->data + 32, init_y, 32);
}

void bench_scalar_add(void* arg) {
    int i;
    bench_inv *data = (bench_inv*)arg;

    for (i = 0; i < 2000000; i++) {
        secp256k1_scalar_add(&data->scalar_x, &data->scalar_x, &data->scalar_y);
    }
}

void bench_scalar_negate(void* arg) {
    int i;
    bench_inv *data = (bench_inv*)arg;

    for (i = 0; i < 2000000; i++) {
        secp256k1_scalar_negate(&data->scalar_x, &data->scalar_x);
    }
}

void bench_scalar_sqr(void* arg) {
    int i;
    bench_inv *data = (bench_inv*)arg;

    for (i = 0; i < 200000; i++) {
        secp256k1_scalar_sqr(&data->scalar_x, &data->scalar_x);
    }
}

void bench_scalar_mul(void* arg) {
    int i;
    bench_inv *data = (bench_inv*)arg;

    for (i = 0; i < 200000; i++) {
        secp256k1_scalar_mul(&data->scalar_x, &data->scalar_x, &data->scalar_y);
    }
}

#ifdef USE_ENDOMORPHISM
void bench_scalar_split(void* arg) {
    int i;
    bench_inv *data = (bench_inv*)arg;

    for (i = 0; i < 20000; i++) {
        secp256k1_scalar l, r;
        secp256k1_scalar_split_lambda(&l, &r, &data->scalar_x);
        secp256k1_scalar_add(&data->scalar_x, &data->scalar_x, &data->scalar_y);
    }
}
#endif

void bench_scalar_inverse(void* arg) {
    int i;
    bench_inv *data = (bench_inv*)arg;

    for (i = 0; i < 2000; i++) {
        secp256k1_scalar_inverse(&data->scalar_x, &data->scalar_x);
        secp256k1_scalar_add(&data->scalar_x, &data->scalar_x, &data->scalar_y);
    }
}

void bench_scalar_inverse_var(void* arg) {
    int i;
    bench_inv *data = (bench_inv*)arg;

    for (i = 0; i < 2000; i++) {
        secp256k1_scalar_inverse_var(&data->scalar_x, &data->scalar_x);
        secp256k1_scalar_add(&data->scalar_x, &data->scalar_x, &data->scalar_y);
    }
}

void bench_field_normalize(void* arg) {
    int i;
    bench_inv *data = (bench_inv*)arg;

    for (i = 0; i < 2000000; i++) {
        secp256k1_fe_normalize(&data->fe_x);
    }
}

void bench_field_normalize_weak(void* arg) {
    int i;
    bench_inv *data = (bench_inv*)arg;

    for (i = 0; i < 2000000; i++)