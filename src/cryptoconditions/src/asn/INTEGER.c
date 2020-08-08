/*-
 * Copyright (c) 2003-2014 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <INTEGER.h>
#include <asn_codecs_prim.h>	/* Encoder and decoder of a primitive type */
#include <errno.h>

/*
 * INTEGER basic type description.
 */
static const ber_tlv_tag_t asn_DEF_INTEGER_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (2 << 2))
};
asn_TYPE_descriptor_t asn_DEF_INTEGER = {
	"INTEGER",
	"INTEGER",
	ASN__PRIMITIVE_TYPE_free,
	INTEGER_print,
	asn_generic_no_constraint,
	ber_decode_primitive,
	INTEGER_encode_der,
	INTEGER_decode_xer,
	INTEGER_encode_xer,
#ifdef	ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	INTEGER_decode_uper,	/* Unaligned PER decoder */
	INTEGER_encode_uper,	/* Unaligned PER encoder */
#endif	/* ASN_DISABLE_PER_SUPPORT */
	0, /* Use generic outmost tag fetcher */
	asn_DEF_INTEGER_tags,
	sizeof(asn_DEF_INTEGER_tags) / sizeof(asn_DEF_INTEGER_tags[0]),
	asn_DEF_INTEGER_tags,	/* Same as above */
	sizeof(asn_DEF_INTEGER_tags) / sizeof(asn_DEF_INTEGER_tags[0]),
	0,	/* No PER visible constraints */
	0, 0,	/* No members */
	0	/* No specifics */
};

/*
 * Encode INTEGER type using DER.
 */
asn_enc_rval_t
INTEGER_encode_der(asn_TYPE_descriptor_t *td, void *sptr,
	int tag_mode, ber_tlv_tag_t tag,
	asn_app_consume_bytes_f *cb, void *app_key) {
	INTEGER_t *st = (INTEGER_t *)sptr;

	ASN_DEBUG("%s %s as INTEGER (tm=%d)",
		cb?"Encoding":"Estimating", td->name, tag_mode);

	/*
	 * Canonicalize integer in the buffer.
	 * (Remove too long sign extension, remove some first 0x00 bytes)
	 */
	if(st->buf) {
		uint8_t *buf = st->buf;
		uint8_t *end1 = buf + st->size - 1;
		int shift;

		/* Compute the number of superfluous leading bytes */
		for(; buf < end1; buf++) {
			/*
			 * If the contents octets of an integer value encoding
			 * consist of more than one octet, then the bits of the
			 * first octet and bit 8 of the second octet:
			 * a) shall not all be ones; and
			 * b) shall not all be zero.
			 */
			switch(*buf) {
			case 0x00: if((buf[1] & 0x80) == 0)
					continue;
				break;
			case 0xff: if((buf[1] & 0x80))
					continue;
				break;
			}
			break;
		}

		/* Remove leading superfluous bytes from the integer */
		shift = buf - st->buf;
		if(shift) {
			uint8_t *nb = st->buf;
			uint8_t *end;

			st->size -= shift;	/* New size, minus bad bytes */
			end = nb + st->size;

			for(; nb < end; nb++, buf++)
				*nb = *buf;
		}

	} /* if(1) */

	return der_encode_primitive(td, sptr, tag_mode, tag, cb, app_key);
}

static const asn_INTEGER_enum_map_t *INTEGER_map_enum2value(asn_INTEGER_specifics_t *specs, const char *lstart, const char *lstop);

/*
 * INTEGER specific human-readable output.
 */
static ssize_t
INTEGER__dump(const asn_TYPE_descriptor_t *td, const INTEGER_t *st, asn_app_consume_bytes_f *cb, void *app_key, int plainOrXER) {
	asn_INTEGER_specifics_t *specs=(asn_INTEGER_specifics_t *)td->specifics;
	char scratch[32];	/* Enough for 64-bit integer */
	uint8_t *buf = st->buf;
	uint8_t *buf_end = st->buf + st->size;
	signed long value;
	ssize_t wrote = 0;
	char *p;
	int ret;

	if(specs && specs->field_unsigned)
		ret = asn_INTEGER2ulong(st, (unsigned long *)&value);
	else
		ret = asn_INTEGER2long(st, &value);

	/* Simple case: the integer size is small */
	if(ret == 0) {
		const asn_INTEGER_enum_map_t *el;
		size_t scrsize;
		char *scr;

		el = (value >= 0 || !specs || !specs->field_unsigned)
			? INTEGER_map_value2enum(specs, value) : 0;
		if(el) {
			scrsize = el->enum_len + 32;
			scr = (char *)alloca(scrsize);
			if(plainOrXER == 0)
				ret = snprintf(scr, scrsize,
					"%ld (%s)", value, el->enum_name);
			else
				ret = snprintf(scr, scrsize,
					"<%s/>", el->enum_name);
		} else if(plainOrXER && specs && specs->strict_enumeration) {
			ASN_DEBUG("ASN.1 forbids dealing with "
				"unknown value of ENUMERATED type");
			errno = EPERM;
			return -1;
		} else {
			scrsize = sizeof(scratch);
			scr = scratch;
			ret = snprintf(scr, scrsize,
				(specs && specs->field_unsigned)
				?"%lu":"%ld", value);
		}
		assert(ret > 0 && (size_t)ret < scrsize);
		return (cb(scr, ret, app_key) < 0) ? -1 : ret;
	} else if(plainOrXER && specs && specs->strict_enumeration) {
		/*
		 * Here and earlier, we cannot encode the ENUMERATED values
		 * if there is no corresponding identifier.
		 */
		ASN_DEBUG("ASN.1 forbids dealing with "
			"unknown value of ENUMERATED type");
		errno = EPERM;
		return -1;
	}

	/* Output in the long xx:yy:zz... format */
	/* TODO: replace with generic algorithm (Knuth TAOCP Vol 2, 4.3.1) */
	for(p = scratch; buf < buf_end; buf++) {
		const char * const h2c = "0123456789ABCDEF";
		if((p - scratch) >= (ssize_t)(sizeof(scratch) - 4)) {
			/* Flush buffer */
			if(cb(scratch, p - scratch, app_key) < 0)
				return -1;
			wrote += p - scratch;
			p = scratch;
		}
		*p++ = h2c[*buf >> 4];
		*p++ = h2c[*buf & 0x0F];
		*p++ = 0x3a;	/* ":" */
	}
	if(p != scratch)
		p--;	/* Remove the last ":" */

	wrote += p - scratch;
	return (cb(scratch, p - scratch, app_key) < 0) ? -1 : wrote;
}

/*
 * INTEGER specific human-readable output.
 */
int
INTEGER_print(asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
	asn_app_consume_bytes_f *cb, void *app_key) {
	const INTEGER_t *st = (const INTEGER_t *)sptr;
	ssize_t ret;

	(void)td;
	(void)ilevel;

	if(!st || !st->buf)
		ret = cb("<absent>", 8, app_key);
	else
		ret = INTEGER__dump(td, st, cb, app_key, 0);

	return (ret < 0) ? -1 : 0;
}

struct e2v_key {
	const char *start;
	const char *stop;
	const asn_INTEGER_enum_map_t *vemap;
	const unsigned int *evmap;
};
static int
INTEGER__compar_enum2value(const void *kp, const void *am) {
	const struct e2v_key *key = (const struct e2v_key *)kp;
	const asn_INTEGER_enum_map_t *el = (const asn_INTEGER_enum_map_t *)am;
	const char *ptr, *end, *name;

	/* Remap the element (sort by different criterion) */
	el = key->vemap + key->evmap[el 