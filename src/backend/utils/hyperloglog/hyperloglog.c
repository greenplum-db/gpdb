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

#include "utils/hyperloglog/hyperloglog.h"


/* ------------- function declarations for local functions --------------- */
static double hll_estimate_dense(HLLCounter hloglog);
static double error_estimate(double E,int b);

static HLLCounter hll_add_hash_dense(HLLCounter hloglog, uint64_t hash);

static HLLCounter hll_compress_dense(HLLCounter hloglog);
static HLLCounter hll_compress_dense_unpacked(HLLCounter hloglog);
static HLLCounter hll_decompress_unpacked(HLLCounter hloglog);
static HLLCounter hll_decompress_dense(HLLCounter hloglog);
static HLLCounter hll_decompress_dense_unpacked(HLLCounter hloglog);

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

	hloglog = hll_decompress_dense_unpacked(hloglog);

	return hloglog;
}


/* Decompresses dense counters */
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
    length = hll_get_size(ndistinct, error);
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

    p->idx = -1;

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
	int upper_bound = POW2(result->b);

	/* check compatibility first */
	//if (counter1->b != counter2->b && -1*counter1->b != counter2->b)
	//elog(ERROR, "index size of estimators differs (%d != %d)", counter1->b, counter2->b);
	//else if (counter1->binbits != counter2->binbits)
	//elog(ERROR, "bin size of estimators differs (%d != %d)", counter1->binbits, counter2->binbits);


	/* Keep the maximum register value for each bin */
	for (i = 0; i < upper_bound; i += 1){
		result->data[i] = ((counter2->data[i] > result->data[i]) ? counter2->data[i] : result->data[i]);
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

/* Hyperloglog estimate header function */
double 
hll_estimate(HLLCounter hloglog)
{
    double E = 0;
    
	E = hll_estimate_dense(hloglog);

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

/* Add element header function */
HLLCounter
hll_add_element(HLLCounter hloglog, const char * element, int elen)
{

    uint64_t hash;

    /* compute the hash */
    hash = MurmurHash64A(element, elen, HASH_SEED);    

    /* add the hash to the estimator */
    hloglog = hll_add_hash_dense(hloglog, hash);

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

/* Just reset the counter (set all the counters to 0). We do this by
 * zeroing the data array */
void 
hll_reset_internal(HLLCounter hloglog)
{

    memset(hloglog->data, 0, VARSIZE_ANY(hloglog) - sizeof(HLLData) );

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


/* Decompress header function */
HLLCounter
hll_decompress(HLLCounter hloglog)
{
     /* make sure the data is compressed */
    if (hloglog->b > 0) {
        return hloglog;
    }
    
    hloglog = hll_decompress_dense(hloglog);

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

/* ---------------------- function definitions --------------------------- */

HLLCounter
hyperloglog_add_item(HLLCounter hllcounter, Datum element, int16 typlen, bool typbyval, char typalign)
{
	HLLCounter hyperloglog;
	
	/* requires the estimator to be already created */
	if (hllcounter == NULL)
		elog(ERROR, "hyperloglog counter must not be NULL");
	
	/* estimator (we know it's not a NULL value) */
	hyperloglog = (HLLCounter) hllcounter;
	
	/* TODO The requests for type info shouldn't be a problem (thanks to
	 * lsyscache), but if it turns out to have a noticeable impact it's
	 * possible to cache that between the calls (in the estimator).
	 *
	 * I have noticed no measurable effect from either option. */
	
	/* decompress if needed */
	if(hyperloglog->b < 0)
	{
		hyperloglog = hll_decompress(hyperloglog);
	}
	
	/* it this a varlena type, passed by reference or by value ? */
	if (typlen == -1)
	{
		/* varlena */
		hyperloglog = hll_add_element(hyperloglog, VARDATA_ANY(element), VARSIZE_ANY_EXHDR(element));
	}
	else if (typbyval)
	{
		/* fixed-length, passed by value */
		hyperloglog = hll_add_element(hyperloglog, (char*)&element, typlen);
	}
	else
	{
		/* fixed-length, passed by reference */
		hyperloglog = hll_add_element(hyperloglog, (char*)element, typlen);
	}
	
	return hyperloglog;
}


double
hyperloglog_estimate(HLLCounter hyperloglog)
{
	double estimate;
	
	/* unpack if needed */
	hyperloglog = hll_unpack(hyperloglog);
	
	estimate = hll_estimate(hyperloglog);
	
	/* return the updated bytea */
	return estimate;
}

HLLCounter
hyperloglog_merge_counters(HLLCounter counter1, HLLCounter counter2)
{
	if (counter1 == NULL && counter2 == NULL)
	{
		/* if both counters are null return null */
		return NULL;
	}
	else if (counter1 == NULL)
	{
		/* if first counter is null just copy the second estimator into the
		 * first one */
		counter1 = hll_copy(counter2);
	}
	else if (counter2 == NULL) {
		/* if second counter is null just return the the first estimator */
		counter1 = hll_copy(counter1);
	}
	else
	{
		/* ok, we already have the estimator - merge the second one into it */
		/* unpack if needed */
		counter1 = hll_unpack(counter1);
		counter2 = hll_unpack(counter2);
		
		/* perform the merge */
		counter1 = hll_merge(counter1, counter2);
	}
	
	/* return the updated HLLCounter */
	return counter1;
}


HLLCounter
hyperloglog_init_def()
{
	HLLCounter hyperloglog;
	
	hyperloglog = hll_create(DEFAULT_NDISTINCT, DEFAULT_ERROR, PACKED);
	
	return hyperloglog;
}

int
hyperloglog_len(HLLCounter hyperloglog)
{
	return VARSIZE_ANY(hyperloglog);
}

/* MurmurHash64A produces the fastest 64 bit hash of the MurmurHash 
 * implementations and is ~ 20x faster than md5. This version produces the
 * same hash for the same key and seed in both big and little endian systems
 * */
uint64_t 
MurmurHash64A (const void * key, int len, unsigned int seed) 
{
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;
    uint64_t h = seed ^ (len * m);
    const uint8_t *data = (const uint8_t *)key;
    const uint8_t *end = data + (len-(len&7));

    while(data != end) {
        uint64_t k;

#if (BYTE_ORDER == LITTLE_ENDIAN)
        k = *((uint64_t*)data);
#else
        k = (uint64_t) data[0];
        k |= (uint64_t) data[1] << 8;
        k |= (uint64_t) data[2] << 16;
        k |= (uint64_t) data[3] << 24;
        k |= (uint64_t) data[4] << 32;
        k |= (uint64_t) data[5] << 40;
        k |= (uint64_t) data[6] << 48;
        k |= (uint64_t) data[7] << 56;
#endif

        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
        data += 8;
    }

    switch(len & 7) {
        case 7: h ^= (uint64_t)data[6] << 48;
        case 6: h ^= (uint64_t)data[5] << 40;
        case 5: h ^= (uint64_t)data[4] << 32;
        case 4: h ^= (uint64_t)data[3] << 24;
        case 3: h ^= (uint64_t)data[2] << 16;
        case 2: h ^= (uint64_t)data[1] << 8;
        case 1: h ^= (uint64_t)data[0];
        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

static const char _base64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int8 b64lookup[128] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
};


int
hll_b64_encode(const char *src, unsigned len, char *dst)
{
	char	   *p,
		*lend = dst + 76;
	const char *s,
		*end = src + len;
	int			pos = 2;
	uint32		buf = 0;

	s = src;
	p = dst;


	while (s < end)
	{
		buf |= (unsigned char)*s << (pos << 3);
		pos--;
		s++;

		/* write it out */
		if (pos < 0)
		{
			*p++ = _base64[(buf >> 18) & 0x3f];
			*p++ = _base64[(buf >> 12) & 0x3f];
			*p++ = _base64[(buf >> 6) & 0x3f];
			*p++ = _base64[buf & 0x3f];

			pos = 2;
			buf = 0;
		}
		if (p >= lend)
		{
			*p++ = '\n';
			lend = p + 76;
		}
	}
	if (pos != 2)
	{
		*p++ = _base64[(buf >> 18) & 0x3f];
		*p++ = _base64[(buf >> 12) & 0x3f];
		*p++ = (pos == 0) ? _base64[(buf >> 6) & 0x3f] : '=';
		*p++ = '=';
	}

	return p - dst;
}

int
hll_b64_decode(const char *src, unsigned len, char *dst)
{
	const char *srcend = src + len,
		*s = src;
	char	   *p = dst;
	char		c;
	int			b = 0;
	uint32		buf = 0;
	int			pos = 0,
		end = 0;


	while (s < srcend)
	{
		c = *s++;

		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			continue;

		if (c == '=')
		{
			/* end sequence */
			if (!end)
			{
				if (pos == 2)
					end = 1;
				else if (pos == 3)
					end = 2;
				else
					ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("unexpected \"=\"")));
			}
			b = 0;
		}
		else
		{
			b = -1;
			if (c > 0 && c < 127)
				b = b64lookup[(unsigned char)c];
			if (b < 0)
				ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("invalid symbol")));
		}
		/* add it to buffer */
		buf = (buf << 6) + b;
		pos++;
		if (pos == 4)
		{
			*p++ = (buf >> 16) & 255;
			if (end == 0 || end > 1)
				*p++ = (buf >> 8) & 255;
			if (end == 0 || end > 2)
				*p++ = buf & 255;
			buf = 0;
			pos = 0;
		}
	}

	if (pos != 0)
		ereport(ERROR,
		(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		errmsg("invalid end sequence")));

	return p - dst;
}


int
b64_enc_len(const char *src, unsigned srclen)
{
	/* 3 bytes will be converted to 4, linefeed after 76 chars */
	return (srclen + 2) * 4 / 3 + srclen / (76 * 3 / 4);
}

int
b64_dec_len(const char *src, unsigned srclen)
{
	return (srclen * 3) >> 2;
}

