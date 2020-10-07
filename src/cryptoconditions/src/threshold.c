/******************************************************************************
 * Copyright © 2014-2020 The Komodo Platform Developers.                      *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * Komodo Platform software, including this file may be copied, modified,     *
 * propagated or distributed except according to the terms contained in the   *
 * LICENSE file.                                                              *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "asn/Condition.h"
#include "asn/Fulfillment.h"
#include "asn/ThresholdFingerprintContents.h"
#include "asn/OCTET_STRING.h"
#include "include/cJSON.h"
#include "cryptoconditions.h"
#include "internal.h"


struct CCType CC_ThresholdType;


static uint32_t thresholdSubtypes(const CC *cond) {
    uint32_t mask = 0;
    for (int i=0; i<cond->size; i++) {
        mask |= cc_typeMask(cond->subconditions[i]);
    }
    mask &= ~(1 << CC_Threshold);
    return mask;
}


static int cmpCostDesc(const void *a, const void *b)
{
    int retval;
    retval = (int) ( *(unsigned long*)b - *(unsigned long*)a );
    return(retval);
    /*if ( retval != 0 )
        return(retval);
    else if ( (uint64_t)a < (uint64_t)b ) // jl777 prevent nondeterminism
        return(-1);
    else return(1);*/
}


static unsigned long thresholdCost(const CC *cond) {
    CC *sub;
    unsigned long *costs = calloc(1, cond->size * sizeof(unsigned long));
    for (int i=0; i<cond->size; i++) {
        sub = cond->subconditions[i];
        costs[i] = cc_getCost(sub);
    }
    qsort(costs, cond->size, sizeof(unsigned long), cmpCostDesc);
    unsigned long cost = 0;
    for (int i=0; i<cond->threshold; i++) {
        cost += costs[i];
    }
    free(costs);
    return cost + 1024 * cond->size;
}


static int thresholdVisitChildren(CC *cond, CCVisitor visitor) {
    for (int i=0; i<cond->size; i++) {
        if (!cc_visit(cond->subconditions[i], visitor)) {
            return 0;
        }
    }
    return 1;
}


static int cmpConditionBin(const void *a, const void *b) {
    /* Compare conditions by their ASN binary representation */
    unsigned char bufa[BUF_SIZE], bufb[BUF_SIZE];
    asn_enc_rval_t r0 = der_encode_to_buffer(&asn_DEF_Condition, *(Condition_t**)a, bufa, BUF_SIZE);
    asn_enc_rval_t r1 = der_encode_to_buffer(&asn_DEF_Condition, *(Condition_t**)b, bufb, BUF_SIZE);

    // below copied from ASN lib
    size_t commonLen = r0.encoded < r1.encoded ? r0.encoded : r1.encoded;
    int ret = memcmp(bufa, bufb, commonLen);

    if (ret == 0)
        return r0.encoded < r1.encoded ? -1 : 1;
    //else if ( (uint64_t)a < (uint64_t)b ) // jl777 prevent nondeterminism
    //    return(-1);
    //else return(1);
    return(0);
}


static unsigned char *thresholdFingerprint(const CC *cond) {
    /* Create fingerprint */
    ThresholdFingerprintContents_t *fp = calloc(1, sizeof(ThresholdFingerprintContents_t));
    //fprintf(stderr,"thresholdfinger %p\n",fp);
    fp->threshold = cond->threshold;
    for (int i=0; i<cond->size; i++) {
        Condition_t *asnCond = asnConditionNew(cond->subconditions[i]);
        asn_set_add(&fp->subconditions2, asnCond);
    }
    qsort(fp->subconditions2.list.array, cond->size, sizeof(Condition_t*), cmpConditionBin);
    return hashFingerprintContents(&asn_DEF_ThresholdFingerprintContents, fp);
}


static int cmpConditionCost(const void *a, const void *b) {
    CC *ca = *((CC**)a);
    CC *cb = *((CC**)b);

    int out = cc_getCost(ca) - cc_getCost(cb);
    if (out != 0) return out;

    // Do an additional sort to establish consistent order
    // between conditions with the same cost.
    Condition_t *asna = asnConditionNew(ca);
    Condition_t *asnb = asnConditionNew(cb);
    out = cmpConditionBin(&asna, &asnb);
    ASN_STRUCT_FREE(asn_DEF_Condition, asna);
    ASN_STRUCT_FREE(asn_DEF_Condition, asnb);
    return out;
}


static CC *thresholdFromFulfillment(const Fulfillment_t *ffill) {
    ThresholdFulfillment_t *t = ffill->choice.thresholdSha256;
    int threshold = t->subfulfillments.list.count;
    int size = threshold + t->subconditions.list.count;

    CC **subconditions = calloc(size, sizeof(CC*));

    for (int i=0; i<size; i++) {
        subconditions[i]