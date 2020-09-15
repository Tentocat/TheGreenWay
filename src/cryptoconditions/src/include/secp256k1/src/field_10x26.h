/**********************************************************************
 * Copyright (c) 2013, 2014 Pieter Wuille                             *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef SECP256K1_FIELD_REPR_H
#define SECP256K1_FIELD_REPR_H

#include <stdint.h>

typedef struct {
    /* X = sum(i=0..9, elem[i]*2^26) mod n */
    uint32_t n[10];
#ifdef VERIFY
    int magnitude;
    int normalized;
#endif
} secp256k1_fe;

/* Unpacks a constant into a overlapping multi-limbed FE element. */
#define SECP256K1_FE_CONST_INNER(d7, d6, d5, d4, d3, d2, d1, d0) { \
    (d0) & 0x3FFFFFFUL, \
    (((uint32_t)d0) >> 26) | (((uint32_t)(d1) & 0xFFFFFUL) << 6), \
    (((uint32_t)d1) >> 20) | (((uint32_t)(d2) & 0x3FFFUL) << 12), \
    (((uint32_t)d2) >> 14) | (((uint32_t)(d3) & 0xFFUL) << 18), \
    (((uint32_t)d3) >> 8) | (((uint32_t)(d4) & 0x3UL) << 24), \
    (((uint32_t)d4) >> 2) & 0x3FFFFFFUL, \
    (((uint32_t)d4) >> 28) | (((uint32_t)(d5) & 0x3FFFFFUL) << 4