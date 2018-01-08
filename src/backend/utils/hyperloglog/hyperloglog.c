/* This file contains internal functions and several functions exposed to the
 * outside via hyperloglog.h. The functions are for the manipulation/creation/
 * evaluation of HLLCounters.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "postgres.h"
#include "utils/pg_lzcompress.h"

#include "utils/varint.h"
#include "utils/hyperloglog.h"
#include "utils/constants.h"
#include "utils/hllutils.h"

/* Externed Constants
 * Alpha * m * m constants, for various numbers of 'b'. See constants.h for
 * more information. */
extern const float alpham[ALPHAM_BOUND];

/* linear counting thresholds */
extern const int threshold[THRESHOLD_BOUND];

/* bit masks used in encode_hash */
extern const uint32_t MASK[NUM_OF_PRECISIONS][NUM_OF_BINWIDTHS];

/* error correction data (x-values) */
extern const double rawEstimateData[NUM_OF_PRECISIONS][MAX_NUM_OF_INTERPOLATION_POINTS];

/* error correction data (y-values) */
extern const double biasData[NUM_OF_PRECISIONS][MAX_NUM_OF_INTERPOLATION_POINTS];

/* precomputed inverse powers of 2 */
extern const double PE[NUM_OF_PRECOMPUTED_EXPONENTS];

/* ------------- function declarations for local functions --------------- */
static double hll_estimate_dense(HLLCounter hloglog);
static double hll_estimate_sparse(HLLCounter hloglog);
static double error_estimate(double E,int b);

static HLLCounter hll_add_hash_dense(HLLCounter hloglog, uint64_t hash);
static HLLCounter hll_add_hash_sparse(HLLCounter hloglog, uint64_t hash);
static uint32_t encode_hash(uint64_t hash, HLLCounter hloglog);
static HLLCounter sparse_to_dense(HLLCounter hloglog);
static HLLCounter sparse_to_dense_unpacked(HLLCounter hloglog);

static HLLCounter hll_compress_dense(HLLCounter hloglog);
static HLLCounter hll_compress_sparse(HLLCounter hloglog);
static HLLCounter hll_compress_dense_unpacked(HLLCounter hloglog);
static HLLCounter hll_decompress_unpacked(HLLCounter hloglog);
static HLLCounter hll_decompress_dense(HLLCounter hloglog);
static HLLCounter hll_decompress_dense_unpacked(HLLCounter hloglog);
static HLLCounter hll_decompress_sparse(HLLCounter hloglog);

static int hll_get_size_sparse(double ndistinct, float error);


/* ---------------------- function definitions --------------------------- */

HLLCounter
hll_unpack(HLLCounter hloglog){

    char entry;
    int i, m;
    HLLCounter htemp;
    
    if (hloglog->format == UNPACKED || hloglog->format == UNPACKED_UNPACKED){
	return hloglog;
    }

    /* use decompress to handle compressed unpacking */
    if (hloglog->b < 0){
	return hll_decompress_unpacked(hloglog);
    }

     /* sparse estimators are unpacked */
    if (hloglog->idx != -1){
        return hloglog;
    }

    /* set format to unpacked*/
    if (hloglog->format == PACKED_UNPACKED){
	hloglog->format = UNPACKED_UNPACKED;
    } else if (hloglog->format == PACKED){
	hloglog->format = UNPACKED;
    }



    /* allocate and zero an array large enough to hold all the decompressed
    * bins */
    m = POW2(hloglog->b);
    htemp = palloc(sizeof(HLLData) + m);
    memcpy(htemp, hloglog, sizeof(HLLData));

	for(i=0; i < m; i++){
	    HLL_DENSE_GET_REGISTER(entry,hloglog->data,i,hloglog->binbits);
	    htemp->data[i] = entry;
	}

    hloglog = htemp;

    /* set the varsize to the appropriate length  */
    SET_VARSIZE(hloglog, sizeof(HLLData) + m);

	return hloglog;
}





HLLCounter
hll_decompress_unpacked(HLLCounter hloglog)
{
	/* make sure the data is compressed */
	if (hloglog->b > 0) {
		return hloglog;
	}

	if (hloglog->idx == -1){
		hloglog = hll_decompress_dense_unpacked(hloglog);
	} else {
		hloglog = hll_decompress_sparse(hloglog);
	}

	return hloglog;
}


/* Decompresses sparse counters */
static HLLCounter
hll_decompress_dense_unpacked(HLLCounter hloglog)
{
	//char * dest;
	int m;
	HLLCounter htemp;

	/* reset b to positive value for calcs and to indicate data is
	* decompressed */
	hloglog->b = -1 * (hloglog->b);
	hloglog->format = UNPACKED;

	/* allocate and zero an array large enough to hold all the decompressed
	* bins */
	m = POW2(hloglog->b);
	htemp = palloc(sizeof(HLLData) + m);
	memset(htemp, 0, m + sizeof(HLLData));
	memcpy(htemp, hloglog, sizeof(HLLData));

	/* decompress the data */
	pglz_decompress((PGLZ_Header *)hloglog->data,(char *) &htemp->data);

	hloglog = htemp;

	/* set the varsize to the appropriate length  */
	SET_VARSIZE(hloglog, sizeof(HLLData) + m);

	return hloglog;
}



/* Allocate HLL estimator that can handle the desired cartinality and
 * precision.
 * 
 * parameters:
 *      ndistinct   - cardinality the estimator should handle
 *      error       - requested error rate (0 - 1, where 0 means 'exact')
 * 
 * returns:
 *      instance of HLL estimator (throws ERROR in case of failure)
 */
HLLCounter
hll_create(double ndistinct, float error, uint8_t format)
{

    float m;
    size_t length;
    HLLCounter p;
    
    /* target error rate needs to be between 0 and 1 */
    if (error <= 0 || error >= 1){
        elog(ERROR, "invalid error rate requested - only values in (0,1) allowed");
    }
    if (MIN_BINBITS >= (uint8_t)ceil(log2(log2(ndistinct))) || MAX_BINBITS <= (uint8_t)ceil(log2(log2(ndistinct)))){
        elog(ERROR,"invalid ndstinct - must be between 257 and 1.1579 * 10^77");
    } 

    /* the counter is allocated as part of this memory block  */
    length = hll_get_size_sparse(ndistinct, error);
    p = (HLLCounter)palloc0(length);

    /* set the counter struct version */
    p->version = STRUCT_VERSION;

	/* set the format to 0 for bitpacked*/
    p->format = format;

    /* what is the minimum number of bins to achieve the requested error rate?
     *  we'll increase this to the nearest power of two later */
    m = ERROR_CONST / (error * error);

    /* so how many bits do we need to index the bins (round up to nearest
     * power of two) */
    p->b = (uint8_t)ceil(log2(m));

    /* set the number of bits per bin */
    p->binbits = (uint8_t)ceil(log2(log2(ndistinct)));

    /* set the starting sparse index to 0 since all counters start sparse and
     *  empty */
    p->idx = 0;

    if (p->b < MIN_INDEX_BITS)   /* we want at least 2^4 (=16) bins */
        p->b = MIN_INDEX_BITS;
    else if (p->b > MAX_INDEX_BITS)
        elog(ERROR, "number of index bits exceeds MAX_INDEX_BITS (requested %d)", p->b);

    SET_VARSIZE(p, length);

    return p;

}

/* Performs a simple 'copy' of the counter, i.e. allocates a new counter and
 * copies the state from the supplied one. */
HLLCounter
hll_copy(HLLCounter counter)
{
    
    size_t length = VARSIZE_ANY(counter);
    HLLCounter copy = (HLLCounter)palloc(length);
    
    memcpy(copy, counter, length);
    
    return copy;

}

/* Merges the two estimators. Either modifies the first estimator in place
 * (inplace=true), or creates a new copy and returns that (inplace=false).
 * Modification in place is very handy in aggregates, when we really want to
 * modify the aggregate state in place.
 * 
 * Merging is only possible if the counters share the same parameters (number
 * of bins, bin size, ...). If the counters don't match, this throws an ERROR.
 *  */
HLLCounter
hll_merge(HLLCounter counter1, HLLCounter counter2)
{

	int i;
	HLLCounter result = counter1;
	uint8_t rho;
	uint32_t * sparse_data, *sparse_data_result, idx;
	int upper_bound = POW2(result->b);

	/* check compatibility first */
	//if (counter1->b != counter2->b && -1*counter1->b != counter2->b)
	//elog(ERROR, "index size of estimators differs (%d != %d)", counter1->b, counter2->b);
	//else if (counter1->binbits != counter2->binbits)
	//elog(ERROR, "bin size of estimators differs (%d != %d)", counter1->binbits, counter2->binbits);


	/* Keep the maximum register value for each bin */
	if (result->idx == -1 && counter2->idx == -1){
		for (i = 0; i < upper_bound; i += 1){

			result->data[i] = ((counter2->data[i] > result->data[i]) ? counter2->data[i] : result->data[i]);

		}
	}
	else if (result->idx == -1) {
		sparse_data = (uint32_t *)counter2->data;

		/* First the encoded hash must be converted to idx and rho before it
		* can be added to the densely encoded result counter */
		for (i = 0; i < counter2->idx; i++){
			idx = sparse_data[i];

			/* if last bit is 1 then rho is the preceding ~6 bits
			* otherwise rho can be calculated from the leading bits*/
			if (sparse_data[i] & 1) {
				/* grab the binbits before the indicator bit and add that to
				* the number of zero bits in p-p' */
				idx = idx >> (32 - result->b);
				rho = ((sparse_data[i] & (int)(POW2(result->binbits + 1) - 2)) >> 1) + (32 - 1 - result->b - result->binbits);
			}
			else {
				idx = (idx << result->binbits) >> result->binbits;
				idx = idx >> (32 - (result->binbits + result->b));
				rho = __builtin_clz(sparse_data[i] << (result->binbits + result->b)) + 1;
			}

			/* keep the highest value */
			if (rho > result->data[idx]) {
				result->data[idx] = rho;
			}

		}
	}
	else if (counter2->idx == -1) {

		result = sparse_to_dense_unpacked(result);
		for (i = 0; i < upper_bound; i += 1){

			result->data[i] = ((counter2->data[i] > result->data[i]) ? counter2->data[i] : result->data[i]);

		}
	}
	else {
		sparse_data = (uint32_t *)counter2->data;
		sparse_data_result = (uint32_t *)result->data;

		/* Add encoded hashes just like in add_hash_sparse with the same
		* dedupe before promotion logic. */
		for (i = 0; i < counter2->idx; i++){
			sparse_data_result[result->idx++] = sparse_data[i];

			if (result->idx > size_sparse_array(result->b)) {
				result->idx = dedupe((uint32_t *)result->data, result->idx);
				if (result->idx > size_sparse_array(result->b) * (7.0 / 8)) {
					result = sparse_to_dense_unpacked(result);
					result = hll_merge(result, counter2);
					return result;
				}
			}
		}

	}

	return result;

}


/* Computes size of the structure, depending on the requested error rate and
 * ndistinct. */
int 
hll_get_size(double ndistinct, float error)
{

    int b;
    float m;
    
    if (error <= 0 || error >= 1)
        elog(ERROR, "invalid error rate requested");
    
    m = ERROR_CONST / (error * error);
    b = (int)ceil(log2(m));
    
    if (b < MIN_INDEX_BITS)
        b = MIN_INDEX_BITS;
    else if (b > MAX_INDEX_BITS)
        elog(ERROR, "number of index bits exceeds MAX_INDEX_BITS (requested %d)",b);
    
    /* The size is the sum of the struct overhead and the bytes the used to 
     * store the buckets. Which is the product of the number of buckets and
     *  the (bits per bucket)/ 8 where 8 is the amount of bits per byte.
     *  
     *  size_in_bytes = struct_overhead + num_buckets*(bits_per_bucket/8)
     *
     * */  
    return sizeof(HLLData) + (int)(ceil(POW2(b)) * ceil(log2(log2(ndistinct))) / 8.0);

}

/* Returns the number of bytes to allocate for the initial sparse encoded 
 * counter. Since palloc allocates blocks that are 2^x bytes big if we were
 * to naively chose our sparse size to be a power of 2 and then add in the 
 * appropriate struct overhead we would have a significant amount of wasted
 * space. So its best to allocate a power of 2 and take the struct overhead 
 * out of the maximum number of sparse encoded hashes. */
static int 
hll_get_size_sparse(double ndistinct, float error)
{

    int b;
    float m;
    if (error <= 0 || error >= 1)
        elog(ERROR, "invalid error rate requested");
    
    m = ERROR_CONST / (error * error);
    b = (int)ceil(log2(m));
    
    if (b < MIN_INDEX_BITS)
        b = MIN_INDEX_BITS;
    else if (b > MAX_INDEX_BITS)
        elog(ERROR, "number of index bits exceeds MAX_INDEX_BITS (requested %d)",b);
    
    return POW2(b-2);
}

/* Hyperloglog estimate header function */
double 
hll_estimate(HLLCounter hloglog)
{
    double E = 0;
    
	if (hloglog->idx == -1 && hloglog->format != PACKED ){
		E = hll_estimate_dense(hloglog);
	} else {
		E = hll_estimate_sparse(hloglog);
	}

    return E;
}
/*
 * Computes the HLL estimate, as described in the paper.
 * 
 * In short it does these steps:
 * 
 * 1) sums the data in counters (1/2^m[i])
 * 2) computes the raw estimate E
 * 3) corrects the estimate for low values
 * 
 */
static double
hll_estimate_dense(HLLCounter hloglog)
{
	double H = 0, E = 0;
	int j, V = 0;
	int m = POW2(hloglog->b);

	/* compute the sum for the harmonic mean */
	if (hloglog->binbits <= MAX_PRECOMPUTED_EXPONENTS_BINWIDTH){
		for (j = 0; j < m; j++){
			H += PE[(int)hloglog->data[j]];
		}
	}
	else {
		for (j = 0; j < m; j++){
			if (0 <= hloglog->data[j] && hloglog->data[j] < NUM_OF_PRECOMPUTED_EXPONENTS){
				H += PE[(int)hloglog->data[j]];
			}
			else {
				H += pow(0.5, hloglog->data[j]);
			}
		}
	}

	/* multiple by constants to turn the mean into an estimate */
	E = alpham[hloglog->b] / H;

	/* correct for hyperloglog's low cardinality bias by either using linear
	*  counting or error estimation */
	if (E <= (5.0 * m)) {

		/* account for hloglog low cardinality bias */
		E = E - error_estimate(E, hloglog->b);

		/* search for empty registers for linear counting */
		for (j = 0; j < m; j++){
			if (hloglog->data[j] == 0){
				V += 1;
			}
		}

		/* Don't use linear counting if there are no empty registers since we
		* don't to divide by 0 */
		if (V != 0){
			H = m * log(m / (float)V);
		}
		else {
			H = E;
		}

		/* if the estimated cardinality is below the threshold for a specific
		* accuracy return the linear counting result otherwise use the error
		* corrected version */
		if (H <= threshold[hloglog->b]) {
			E = H;
		}

	}

	return E;

}


/* Estimates the error from hyperloglog's low cardinality bias and by taking
 * a simple linear regression of the nearest 6 points */
static double 
error_estimate(double E,int b)
{
    double avg=0;
    double beta,alpha;
    double sx,sxx,sxy,sy;
    int i, idx, max=0;

    /* get the number of interpoloation points for that precision */
    if (b > 5 && b <= 18) {
        max = MAX_INTERPOLATION_POINTS;
    } else if (b == 5) {
        max = PRECISION_5_MAX_INTERPOLATION_POINTS;
    } else if (b == 4) {
        max = PRECISION_4_MAX_INTERPOLATION_POINTS;
    } else {
	elog(ERROR,"ERROR: parameter b (%d) is out of range (4-18)",b);
    }

    idx = max;

    /* find the index of the first interpolation point greater than the 
     * uncorrected estimate */
    for (i = 0; i < max; i++){
        if (E < rawEstimateData[b-4][i]){
            idx = i;
            break;
        }
    }

    /* make sure array indexes will be inbounds when getting 6 nearest data
     * points */
    if (idx < 3) {
        idx = 3;
    } else if ( idx > max - 2){
        idx = max - 2;
    }

    /* calculate the alpha and beta needed to interpolate the error correction
     * for E */
    sx = rawEstimateData[b-4][idx+2] + rawEstimateData[b-4][idx+1] + rawEstimateData[b-4][idx] + rawEstimateData[b-4][idx-1] + rawEstimateData[b-4][idx-2] + rawEstimateData[b-4][idx-3];
    sxx = rawEstimateData[b-4][idx+2]*rawEstimateData[b-4][idx+2] + rawEstimateData[b-4][idx+1]*rawEstimateData[b-4][idx+1] + rawEstimateData[b-4][idx]*rawEstimateData[b-4][idx] + rawEstimateData[b-4][idx-1]*rawEstimateData[b-4][idx-1] + rawEstimateData[b-4][idx-2]*rawEstimateData[b-4][idx-2] + rawEstimateData[b-4][idx-3]*rawEstimateData[b-4][idx-3]; 
    sy = biasData[b-4][idx+2] + biasData[b-4][idx+1] + biasData[b-4][idx] + biasData[b-4][idx-1] + biasData[b-4][idx-2] + biasData[b-4][idx-3];
    sxy = rawEstimateData[b-4][idx+2]*biasData[b-4][idx+2] + rawEstimateData[b-4][idx+1]*biasData[b-4][idx+1] + rawEstimateData[b-4][idx]*biasData[b-4][idx] + rawEstimateData[b-4][idx-1]*biasData[b-4][idx-1] + rawEstimateData[b-4][idx-2]*biasData[b-4][idx-2] + rawEstimateData[b-4][idx-3]*biasData[b-4][idx-3];
    beta = (6.0*sxy - sx*sy ) / ( 6.0*sxx - sx*sx );
    alpha = (1.0/6.0)*sy - beta*(1.0/6.0)*sx;

    avg = alpha + E*beta;

    return avg;
}

/* Evaluates the stored encoded hashes using linear counting */
static double 
hll_estimate_sparse(HLLCounter hloglog)
{
    int i,V,m = POW2(32 - 1 - hloglog->binbits);
    uint32_t * sparse_data;

    /* sort the values so we can ignore duplicates */
    hloglog->idx = dedupe((uint32_t *)hloglog->data,hloglog->idx);
    sparse_data = (uint32_t *) hloglog->data;
    
    /* count number of unique values */
    V = 0;
    for (i=0; i < hloglog->idx ; i++){
        if (i == 0){
            V++;
        } else if (sparse_data[i] != sparse_data[i-1]){
            V++;
        }
    }

    /* Instead of counting empty counters like we do in dense estimation with 
     * linear counting we need to count the number of unique values and 
     * subtract that from the total possible unique counters since we are
     * using a sparse representation of this space. */
    return  m * log(m / (double)(m-V));

}

/* Add element header function */
HLLCounter
hll_add_element(HLLCounter hloglog, const char * element, int elen)
{

    uint64_t hash;

    /* compute the hash */
    hash = MurmurHash64A(element, elen, HASH_SEED);    

    /* add the hash to the estimator */
    if (hloglog->idx == -1 ){
        hloglog = hll_add_hash_dense(hloglog, hash);
    } else {
        hloglog = hll_add_hash_sparse(hloglog, hash);
    }

    return hloglog;
}

/* Add the appropriate values to a dense encoded counter for a given hash */
static HLLCounter
hll_add_hash_dense(HLLCounter hloglog, uint64_t hash)
{

    uint64_t idx;
    uint8_t rho,entry,addn;

    /* get idx (keep only the first 'b' bits) */
    idx  = hash >> (HASH_LENGTH - hloglog->b);

    /* rho needs to be independent from 'idx' */
    rho = __builtin_clzll(hash << hloglog->b) + 1;

    /* We only have (64 - hloglog->b) bits leftover after the index bits
     * however the chance that we need more is 2^-(64 - hloglog->b) which
     * is very small. So we only compute more when needed. To do this we
     * rehash the original hash and take the rho of the new hash and add it
     * to the (64 - hloglog->b) bits. We can repeat this for rho up to 255.
     * We can't go any higher since integer values >255 take more than 1 byte
     * which is currently supported nor really necessary due to 2^(2^8) ~ 
     * 1.16E77 a number so large its not feasible to have that many unique 
     * elements. */
    if (rho == HASH_LENGTH){
	    addn = HASH_LENGTH;
	    rho = (HASH_LENGTH - hloglog->b);
	    while (addn == HASH_LENGTH && rho < POW2(hloglog->binbits)){
		    hash = MurmurHash64A((const char * )&hash, HASH_LENGTH/8, HASH_SEED);
            /* zero length runs should be 1 so counter gets set */
		    addn = __builtin_clzll(hash) + 1;
		    rho += addn;
	    }
    }

    /* keep the highest value */
    HLL_DENSE_GET_REGISTER(entry,hloglog->data,idx,hloglog->binbits);
    if (rho > entry) {
        HLL_DENSE_SET_REGISTER(hloglog->data,idx,rho,hloglog->binbits);
    }
    
    return hloglog;

}

/* Add the encoded hash to the array of sparse values and promote to dense 
 * encoding if size threshold is exceeded */
static HLLCounter
hll_add_hash_sparse(HLLCounter hloglog, uint64_t hash)
{
    uint32_t encoded_hash;
    uint32_t * bigdata;

    bigdata = (uint32_t *) hloglog->data;
    encoded_hash = encode_hash(hash,hloglog);

    bigdata[hloglog->idx++] = encoded_hash;

    /* If the size threshold is exceeded first we dedupe the array to guard 
     * against situations where many duplicate elements are input into the 
     * counter which would needlessly promote it to dense encoding sacrificing
     * accuracy at a lower cardinality than desired. However if the dedupe 
     * doesn't provide a reasonable reduction its should still be promoted. */
    if (hloglog->idx > size_sparse_array(hloglog->b)){
        hloglog->idx = dedupe((uint32_t *)hloglog->data,hloglog->idx);
        if (hloglog->idx > size_sparse_array(hloglog->b)*7/8){
            hloglog = sparse_to_dense(hloglog);
        }
    }
    
    return hloglog;
}

/* Encode the 64 bit hash to a 32 bit summary for sparse encoding and later
 *  conversion to dense encoding. The encoding scheme is as follows:
 * 
 * We take an index of 25 bits (as opposed to the default 14) to allow for
 * greater accuracy when sparsely encoded. If the bits 14 - 25 contain a 1 
 * (so rho is less than 11) then we add a 0 to the of the encode as a flag 
 * for this. However if these bits are all 0's then we need to add rho to the
 * end of this so we can sucesfully switch to dense encoding when the time 
 * comes. We indicate this scenario with a trailing 1 bit. It is worth noting
 * we actually add rho - 11 since we can infer that extra 11 from the fact
 * that bits 14-25 are 0.
 *
 * Scenario 1 (where rho is in bits 14-25):
 * six 0 bits | first 25 bits of hash | 0 = 32 bit encoded number
 *
 * Scenario 2 (where rho is outside of bits 14-25):
 * first 25 bits of hash | rho - 11 (6 bits total) | 1 = 32 bit encoded number
*/
static uint32_t 
encode_hash(uint64_t hash, HLLCounter hloglog)
{
    uint32_t encode = 0;
    uint64_t idx;
    uint8_t rho,addn;

    /* which stream is this (keep only the first p' bits) */
    idx  = hash >> (HASH_LENGTH - (32 - 1 - hloglog->binbits));

    /* check p-p' bits for significant digit */
    if (idx & MASK[hloglog->b - 4][hloglog->binbits - 4]){
        encode = idx << 1;
    } else {
        encode = idx << hloglog->binbits;
        rho = __builtin_clzll(hash<<(32- 1 - hloglog->binbits)) + 1;
        if (rho == HASH_LENGTH){
            addn = HASH_LENGTH;
            rho = (HASH_LENGTH - (32- 1 - hloglog->binbits));
            while (addn == HASH_LENGTH && rho < POW2(hloglog->binbits)){
                hash = MurmurHash64A((const char * )&hash, HASH_LENGTH/8, HASH_SEED);
                /*zero length runs should be 1 so counter gets set */
                addn = __builtin_clzll(hash) + 1;
                rho += addn;
            }
        }
        encode = encode + rho;
        encode = (encode << 1);
        encode = encode + 1;
    }

    return encode;
}

/* Promote a counter from sparse encoding to dense encoding */
static HLLCounter
sparse_to_dense(HLLCounter hloglog)
{
    HLLCounter htemp;
    uint32_t * sparse_data;
    uint32_t idx;
    uint8_t rho,entry;
    int i, m = POW2(hloglog->b);

    if (hloglog->idx == -1){
        return hloglog;
    }

    /* Sparse encoded counters are smaller than dense so new space needs to be
     *  alloced */
    sparse_data = malloc(hloglog->idx*sizeof(uint32_t));
    if (sparse_data == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("out of memory"),
                 errdetail("Failed on request of size %zu.", hloglog->idx*sizeof(uint32_t))));

    memmove(sparse_data,&hloglog->data,hloglog->idx*sizeof(uint32_t));
    htemp = palloc0(sizeof(HLLData) + (int)ceil((m * hloglog->binbits / 8.0)));
    memcpy(htemp,hloglog,sizeof(HLLData));
    hloglog = htemp;
    memset(hloglog->data,0,(int)ceil((m * hloglog->binbits / 8.0)));

    for (i=0; i < hloglog->idx; i++){
        idx = sparse_data[i];

        /* if last bit is 1 then rho is the preceding ~6 bits
         * otherwise rho can be calculated from the leading bits*/
        if (sparse_data[i] & 1) {
            /* grab the binbits before the indicator bit and add that to the 
             * number of zero bits in p-p' */
            idx = idx >> (32 - hloglog->b);
            rho = ((sparse_data[i] & (int)(POW2(hloglog->binbits+1) - 2)) >> 1) + (32 - 1 - hloglog->b - hloglog->binbits);
        } else {
            idx = (idx << hloglog->binbits) >> hloglog->binbits;
            idx  = idx >> (32 - (hloglog->binbits+ hloglog->b));
            rho = __builtin_clz(sparse_data[i] << (hloglog->binbits+ hloglog->b)) + 1;
        }
        
        /* keep the highest value */
        HLL_DENSE_GET_REGISTER(entry,hloglog->data,idx,hloglog->binbits);
        if (rho > entry) {
            HLL_DENSE_SET_REGISTER(hloglog->data,idx,rho,hloglog->binbits);
        }

    }
    
    if (sparse_data){
        free(sparse_data);
    }
    
    SET_VARSIZE(hloglog,sizeof(HLLData) + (int)ceil((m * hloglog->binbits / 8.0)) );

    hloglog->idx = -1;

    return hloglog;
}

static HLLCounter
sparse_to_dense_unpacked(HLLCounter hloglog)
{
	HLLCounter htemp;
	uint32_t * sparse_data;
	uint32_t idx;
	uint8_t rho;
	int i, m = POW2(hloglog->b);

	if (hloglog->idx == -1){
		return hloglog;
	}

        if (hloglog->format == PACKED){
	    hloglog->format = UNPACKED;
        } else if (hloglog->format == PACKED_UNPACKED) {
            hloglog->format = UNPACKED_UNPACKED;
        } else {
           elog(ERROR,"Sparse counter should either be PACKED or PACKED_UNPACKED it is:%d",hloglog->format);
        }

	/* Sparse encoded counters are smaller than dense so new space needs to be
	*  alloced */
	sparse_data = malloc(hloglog->idx*sizeof(uint32_t));
	if (sparse_data == NULL)
		ereport(ERROR,
		(errcode(ERRCODE_OUT_OF_MEMORY),
		errmsg("out of memory"),
		errdetail("Failed on request of size %zu.", hloglog->idx*sizeof(uint32_t))));

	memmove(sparse_data, &hloglog->data, hloglog->idx*sizeof(uint32_t));
	htemp = palloc0(sizeof(HLLData) + m);
	memcpy(htemp, hloglog, sizeof(HLLData));
	hloglog = htemp;
	memset(hloglog->data, 0, m);
	int maxidx = hloglog->idx;

	for (i = 0; i < maxidx; i++){
		idx = sparse_data[i];

		/* if last bit is 1 then rho is the preceding ~6 bits
		* otherwise rho can be calculated from the leading bits*/
		if (sparse_data[i] & 1) {
			/* grab the binbits before the indicator bit and add that to the
			* number of zero bits in p-p' */
			idx = idx >> (32 - hloglog->b);
			rho = ((sparse_data[i] & (int)(POW2(hloglog->binbits + 1) - 2)) >> 1) + (32 - 1 - hloglog->b - hloglog->binbits);
		}
		else {
			idx = (idx << hloglog->binbits) >> hloglog->binbits;
			idx = idx >> (32 - (hloglog->binbits + hloglog->b));
			rho = __builtin_clz(sparse_data[i] << (hloglog->binbits + hloglog->b)) + 1;
		}

		/* keep the highest value */
    	if (rho > hloglog->data[idx]) {
			hloglog->data[idx] = rho;
		}

	}

	if (sparse_data){
		free(sparse_data);
	}

	SET_VARSIZE(hloglog, sizeof(HLLData) + m);

	hloglog->idx = -1;

	return hloglog;
}

/* Just reset the counter (set all the counters to 0). We do this by
 * zeroing the data array */
void 
hll_reset_internal(HLLCounter hloglog)
{

    memset(hloglog->data, 0, VARSIZE_ANY(hloglog) - sizeof(HLLData) );

}

/* check the equality by comparing the register values not the cardinalities */
int
hll_is_equal(HLLCounter counter1, HLLCounter counter2)
{

    HLLCounter counter1copy,counter2copy;
    uint32_t * sparse_data1, *sparse_data2;
    int i, m = POW2(counter1->b);

    /* check compatibility first */
    if (counter1->b != counter2->b)
        elog(ERROR, "index size (bit length) of estimators differs (%d != %d)", counter1->b, counter2->b);
    else if (counter1->binbits != counter2->binbits)
        elog(ERROR, "bin size of estimators differs (%d != %d)", counter1->binbits, counter2->binbits);
    else if (((counter1->format == PACKED || counter1->format == PACKED_UNPACKED) && counter1->idx == -1) || ((counter2->format == PACKED || counter2->format == PACKED_UNPACKED) && counter2->idx == -1))
	elog(ERROR, "Estimator(s) are not unpacked! (%d,%d)", counter1->format, counter2->format);

    /* compare registers returning false on any difference */
    if (counter1->idx == -1 && counter2->idx == -1){
        for (i = 0; i < m; i++){
            if (counter1->data[i] != counter2->data[i]){
                return 0;
            }
        }
    } else if (counter1->idx == -1) {
        counter2copy = sparse_to_dense_unpacked(counter2);
        for (i = 0; i < m; i++){
            if (counter1->data[i] != counter2copy->data[i]){
                return 0;
            }
        }
    } else if (counter2->idx == -1) {
        counter1copy = sparse_to_dense_unpacked(counter1);
        for (i = 0; i < m; i++){
            if (counter1copy->data[i] != counter2->data[i]){
                return 0;
            }
        }
    } else {
        counter1copy = hll_copy(counter1);
        counter2copy = hll_copy(counter2);
        sparse_data1 = (uint32_t *) counter1copy->data;
        sparse_data2 = (uint32_t *) counter2copy->data;

        counter1copy->idx = dedupe((uint32_t *)counter1copy->data,counter1copy->idx);
        counter2copy->idx = dedupe((uint32_t *)counter2copy->data,counter2copy->idx);

        if (counter1copy->idx != counter2copy->idx){
            return 0;
        }

        for (i=0; i < counter1copy->idx ; i++){
            if (sparse_data1[i] != sparse_data2[i]){
                return 0;
            }
        }

    }

    return 1;

}
/* Compress header function */
HLLCounter
hll_compress(HLLCounter hloglog)
{

    /* make sure the data isn't compressed already */
    if (hloglog->b < 0) {
        return hloglog;
    }

    if (hloglog->idx == -1 && hloglog->format == PACKED){
        hloglog = hll_compress_dense(hloglog);
    } else if (hloglog->idx == -1 && hloglog->format == UNPACKED){
	hloglog = hll_compress_dense_unpacked(hloglog);
    } else if (hloglog->format == UNPACKED_UNPACKED){
	hloglog->format = UNPACKED;
    } else if (hloglog->format == PACKED_UNPACKED){
	hloglog = hll_unpack(hloglog);
    } else if (hloglog->idx != -1) {
        hloglog = hll_compress_sparse(hloglog);
    }
    
    return hloglog;
}

/* Compresses dense encoded counters using lz compression */
static HLLCounter
hll_compress_dense(HLLCounter hloglog)
{
    PGLZ_Header * dest;
    char entry,*data;    
    int i, m;

    /* make sure the dest struct has enough space for an unsuccessful 
     * compression and a 4 bytes of overflow since lz might not recognize its
     * over until then preventing segfaults */
    m = POW2(hloglog->b);
    dest = malloc(m + sizeof(PGLZ_Header) + 4);
    if (dest == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("out of memory"),
                 errdetail("Failed on request of size %zu.", m + sizeof(PGLZ_Header) + 4)));
    memset(dest,0,m + sizeof(PGLZ_Header) + 4);
    data = malloc(m);
    if (data == NULL){
	free(dest);
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("out of memory"),
                 errdetail("Failed on request of size %d.", m)));
    }

    /* put all registers in a normal array  i.e. remove dense packing so
     * lz compression can work optimally */
    for(i=0; i < m ; i++){
        HLL_DENSE_GET_REGISTER(entry,hloglog->data,i,hloglog->binbits);
        data[i] = entry;
    }

    /* lz_compress the normalized array and copy that data into hloglog->data
     * if any compression was acheived */
    pglz_compress(data,m,dest,PGLZ_strategy_always);
    if (VARSIZE_ANY(dest) >= (m * hloglog->binbits /8) ){
	/* free allocated memory and return unaltered array */
    	if (dest){
            free(dest);
    	}
    	if (data){
            free(data);
    	}
    	return hloglog;
    }
    memcpy(hloglog->data,dest,VARSIZE_ANY(dest));

    /* resize the counter to only encompass the compressed data and the struct
     *  overhead*/
    SET_VARSIZE(hloglog,sizeof(HLLData) + VARSIZE_ANY(dest) );

    /* invert the b value so it being < 0 can be used as a compression flag */
    hloglog->b = -1 * (hloglog->b);

    /* free allocated memory */
    if (dest){
        free(dest);
    }
    if (data){
        free(data);
    }

    /* return the compressed counter */
    return hloglog;
}

/* Compresses dense encoded counters using lz compression */
static HLLCounter
hll_compress_dense_unpacked(HLLCounter hloglog)
{
	PGLZ_Header * dest;
	int m;

	/* make sure the dest struct has enough space for an unsuccessful
	* compression and a 4 bytes of overflow since lz might not recognize its
	* over until then preventing segfaults */
	m = POW2(hloglog->b);
	dest = malloc(m + sizeof(PGLZ_Header) + 4);
	if (dest == NULL){
		return 0;
	}
	if (dest == NULL)
		ereport(ERROR,
			(errcode(ERRCODE_OUT_OF_MEMORY),
			 errmsg("out of memory"),
			 errdetail("Failed on request of size %zu.", m + sizeof(PGLZ_Header) + 4)));


	memset(dest, 0, m + sizeof(PGLZ_Header) + 4);


	/* lz_compress the normalized array and copy that data into hloglog->data
	* if any compression was acheived */
	pglz_compress(hloglog->data, m, dest, PGLZ_strategy_always);
	if (VARSIZE_ANY(dest) >= (m * hloglog->binbits / 8)){
		/* free allocated memory and return unaltered array */
		if (dest){
			free(dest);
		}
		return hloglog;
	}
	memcpy(hloglog->data, dest, VARSIZE_ANY(dest));

	/* resize the counter to only encompass the compressed data and the struct
	*  overhead*/
	SET_VARSIZE(hloglog, sizeof(HLLData) + VARSIZE_ANY(dest));

	/* invert the b value so it being < 0 can be used as a compression flag */
	hloglog->b = -1 * (hloglog->b);
	hloglog->format = PACKED;

	/* free allocated memory */
	if (dest){
		free(dest);
	}

	/* return the compressed counter */
	return hloglog;
}

/* Sparse compression uses group-varint encoding on a list of deltas made from
 * a sorted and deduped list of the encoded hashes. Group-varint encoding can
 * be seen in further detail in varint.h but essentially it operates on groups
 * of 4 uint32_t's to compress the chunks. Any remainders is encoded using
 * continuation bit variable length encoding.
 *
 * However if this fails to produce any reduction in size the original deduped
 * and sorted list is stored and this is indicated by
 * hloglog->b = -1*(hloglog->b + MAX_INDEX_BITS)
 *
 * Using MAX_INDEX_BITS ensures no overlap in negative b values which are
 * varint encoded and those that aren't since b <= MAX_INDEX_BITS */
static HLLCounter
hll_compress_sparse(HLLCounter hloglog)
{
    uint32_t out;
    uint8_t * encodes;

    /* Add 80 (320 bytes) to allow for cases where group-varint doesn't reduce
     * size. This allows enough space for worst case scenario compression */
    uint32_t sparse_size = size_sparse_array(hloglog->b) + 80;

    encodes = malloc(sparse_size*sizeof(uint32_t));
    if (encodes == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("out of memory"),
                 errdetail("Failed on request of size %zu.", sparse_size*sizeof(uint32_t))));

    hloglog->idx = dedupe((uint32_t *)hloglog->data,hloglog->idx);

    out = group_encode_sorted((uint32_t *)hloglog->data,hloglog->idx,encodes);

    if (out < hloglog->idx*4){
        memcpy(hloglog->data,encodes,out);
        SET_VARSIZE(hloglog,sizeof(HLLData) + out);
        hloglog->b = -1 * (hloglog->b);
    } else {
        SET_VARSIZE(hloglog,sizeof(HLLData) + hloglog->idx*4);
        hloglog->b = ( -1 * (hloglog->b + MAX_INDEX_BITS));
    }

    if (encodes){
        free(encodes);
    }

    return hloglog;
}

/* Decompress header function */
HLLCounter
hll_decompress(HLLCounter hloglog)
{
     /* make sure the data is compressed */
    if (hloglog->b > 0) {
        return hloglog;
    }
    
    if (hloglog->idx == -1){
        hloglog = hll_decompress_dense(hloglog);
    } else {
        hloglog = hll_decompress_sparse(hloglog);
    }
    
    return hloglog;
}

/* Decompresses dense counters */
static HLLCounter
hll_decompress_dense(HLLCounter hloglog)
{
    char * dest;
    int m,i;
    HLLCounter htemp;

    /* reset b to positive value for calcs and to indicate data is
     * decompressed */
    hloglog->b = -1 * (hloglog->b);

    /* allocate and zero an array large enough to hold all the decompressed 
     * bins */
    m = POW2(hloglog->b);
    dest = malloc(m);
    if (dest == NULL)
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("out of memory"),
                 errdetail("Failed on request of size %d.", m)));

    memset(dest,0,m);

    /* decompress the data */
    pglz_decompress((PGLZ_Header *)hloglog->data,dest);

    /* copy the struct internals but not the data into a counter with enough 
     * space for the uncompressed data  */
    htemp = palloc(sizeof(HLLData) + (int)ceil((m * hloglog->binbits / 8.0)));
    memcpy(htemp,hloglog,sizeof(HLLData));
    hloglog = htemp;

    /* set the registers to the appropriate value based on the decompressed
     * data */
    for (i=0; i<m; i++){
        HLL_DENSE_SET_REGISTER(hloglog->data,i,dest[i],hloglog->binbits);
    }

    /* set the varsize to the appropriate length  */
    SET_VARSIZE(hloglog,sizeof(HLLData) + (int)ceil((m * hloglog->binbits / 8.0)) );
    

    /* free allocated memory */
    if (dest){
        free(dest);
    }

    return hloglog;
}

/* Decompresses sparse counters. To do this first the compression flag is
 * checked to see if group-varint encoding was used. If -b > MAX_BIN_BITS then
 * no compression was used the counter was simply resized so all that needs to
 * be done is to copy the data into a fully allocated chunk of memory.
 * However, if -b < MAX_INDEX_BITS then group-varint encoding was used and the
 * bytes in hloglog->data must be decoded before being copied into a fully
 * allocated chunk of memory as well. */
static HLLCounter
hll_decompress_sparse(HLLCounter hloglog)
{
	HLLCounter htemp;
    size_t length;

    /* reset b to positive value for calcs and to indicate data is
     * decompressed */
    hloglog->b = -1 * (hloglog->b);

    if (hloglog->b > MAX_INDEX_BITS){
        hloglog->b = hloglog->b - MAX_INDEX_BITS;
        
        length = POW2(hloglog->b-2);
        htemp = palloc0(length);
        memcpy(htemp,hloglog,VARSIZE_ANY(hloglog));
        hloglog = htemp;

        SET_VARSIZE(hloglog,length);
    } else {
        length = POW2(hloglog->b-2);
        htemp = palloc0(length);
        memcpy(htemp,hloglog,sizeof(HLLData));
        group_decode_sorted((uint8_t *)hloglog->data,hloglog->idx,(uint32_t *) htemp->data);
        hloglog = htemp;

        SET_VARSIZE(hloglog,length);
    }
    
    return hloglog;
}

