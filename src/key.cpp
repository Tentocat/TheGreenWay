// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2017 The Zcash developers
// Copyright (c) 2020 The SmartUSD Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>

#include <arith_uint256.h>
#include <crypto/common.h>
#include <crypto/hmac_sha512.h>
#include <random.h>
#include <base58.h>

#include <secp256k1.h>
#include <secp256k1_recovery.h>

static secp256k1_context* secp256k1_context_sign = nullptr;

/** These functions are taken from the libsecp256k1 distribution and are very ugly. */

/**
 * This parses a format loosely based on a DER encoding of the ECPrivateKey type from
 * section C.4 of SEC 1 <http://www.secg.org/sec1-v2.pdf>, with the following caveats:
 *
 * * The octet-length of the SEQUENCE must be encoded as 1 or 2 octets. It is not
 *   required to be encoded as one octet if it is less than 256, as DER would require.
 * * The octet-length of the SEQUENCE must not be greater than the remaining
 *   length of the key encoding, but need not match it (i.e. the encoding may contain
 *   junk after the encoded SEQUENCE).
 * * The privateKey OCTET STRING is zero-filled on the left to 32 octets.
 * * Anything after the encoding of the privateKey OCTET STRING is ignored, whether
 *   or not it is validly encoded DER.
 *
 * out32 must point to an output buffer of length at least 32 bytes.
 */
static int ec_privkey_import_der(const secp256k1_context* ctx, unsigned char *out32, const unsigned char *privkey, size_t privkeylen) {
    const unsigned char *end = privkey + privkeylen;
    memset(out32, 0, 32);
    /* sequence header */
    if (end - pr