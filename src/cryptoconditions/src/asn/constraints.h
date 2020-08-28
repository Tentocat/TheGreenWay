/*-
 * Copyright (c) 2004, 2006 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	ASN1_CONSTRAINTS_VALIDATOR_H
#define	ASN1_CONSTRAINTS_VALIDATOR_H

#include "asn_system.h"		/* Platform-dependent types */

#ifdef __cplusplus
extern "C" {
#endif

struct asn_TYPE_descriptor_s;		/* Forward declaration */

/*
 * Validate the structure 