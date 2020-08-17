/*-
 * Copyright (c) 2003, 2004, 2005, 2006 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <OCTET_STRING.h>
#include <BIT_STRING.h>	/* for .bits_unused member */
#include <errno.h>

/*
 * OCTET STRING basic type description.
 */
static const ber_tlv_tag_t asn_DEF_OCTET_STRING_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (4 << 2))
};
static const asn_OCTET_STRING_specifics_t asn_DEF_OCTET_STRING_specs = {
	sizeof(OCTET_STRING_t),
	offsetof(OCTET_STRING_t, _asn_ctx),
	ASN_OSUBV_STR
};
static const asn_per_constraints_t asn_DEF_OCTET_STRING_constraints = {
	{ APC_CONSTRAINED, 8, 8, 0, 255 },
	{ APC_SEMI_CONSTRAINED, -1, -1, 0, 0 },
	0, 0
};
asn_TYPE_descriptor_t asn_DEF_OCTET_STRING = {
	"OCTET STRING",		/* Canonical name */
	"OCTET_STRING",		/* XML tag name */
	OCTET_STRING_free,
	OCTET_STRING_print,	/* non-ascii stuff, generally */
	asn_generic_no_constraint,
	OCTET_STRING_decode_ber,
	OCTET_STRING_encode_der,
	OCTET_STRING_decode_xer_hex,
	OCTET_STRING_encode_xer,
	OCTET_STRING_decode_uper,	/* Unaligned PER decoder */
	OCTET_STRING_encode_uper,	/* Unaligned PER encoder */
	0, /* Use generic outmost tag fetcher */
	asn_DEF_OCTET_STRING_tags,
	sizeof(asn_DEF_OCTET_STRING_tags)
	  / sizeof(asn_DEF_OCTET_STRING_tags[0]),
	asn_DEF_OCTET_STRING_tags,	/* Same as above */
	sizeof(asn_DEF_OCTET_STRING_tags)
	  / sizeof(asn_DEF_OCTET_STRING_tags[0]),
	0,	/* No PER visible constraints */
	0, 0,	/* No members */
	&asn_DEF_OCTET_STRING_specs
};

#undef	_CH_PHASE
#undef	NEXT_PHASE
#undef	PREV_PHASE
#define	_CH_PHASE(ctx, inc) do {					\
		if(ctx->phase == 0)					\
			ctx->context = 0;				\
		ctx->phase += inc;					\
	} while(0)
#define	NEXT_PHASE(ctx)	_CH_PHASE(ctx, +1)
#define	PREV_PHASE(ctx)	_CH_PHASE(ctx, -1)

#undef	ADVANCE
#define	ADVANCE(num_bytes)	do {	