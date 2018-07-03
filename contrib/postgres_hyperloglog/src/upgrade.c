/* Contains the upgrade function and all the necessary legacy functions
 * needed to support this.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "postgres.h"
#include "upgrade.h"
#include "utils/pg_lzcompress.h"

#include "hllutils.h"

/* ------------- function declarations for local functions --------------- */
/* V1 specific function versions */
static HLLCounter hll_decompress_V1(HLLCounter hloglog);
static HLLCounter hll_decompress_dense_V1(HLLCounter hloglog);
static HLLCounter hll_decompress_sparse_V1(HLLCounter hloglog);

/* ---------------------- function definitions --------------------------- */
/* Used to upgrade old versions to the newest version. This is needed when the
 * HyperLogLogCounterData struct changes or how its handled changes. */
HLLCounter 
hll_upgrade(HLLCounter hloglog)
{
    int m;
    HLLCounter htemp = 0;
    if (hloglog->version == 0){
        if (hloglog->b < 0){
            m = pow(2,-1*hloglog->b);
        } else {
            m = pow(2,hloglog->b);
        }
        htemp = palloc0(sizeof(HLLData) + (int)ceil((m * hloglog->binbits / 8)));
        memcpy(htemp->data,(char *)hloglog + 8,(m * hloglog->binbits / 8));
        htemp->b = hloglog->b;
        htemp->binbits = hloglog->binbits;
        htemp->version = STRUCT_VERSION;
        htemp->idx = -1;
        if (hloglog->b < 0){
            htemp = hll_decompress_dense_V1(htemp);
        }
        SET_VARSIZE(htemp, (sizeof(HLLData) +(m * hloglog->binbits / 8)));
        htemp = hll_compress(htemp);
    } else if (hloglog->version == 1){
        if (hloglog->b < 0){
            hloglog = hll_decompress_V1(hloglog);
        }
        hloglog->version = STRUCT_VERSION;
        hloglog = hll_compress(hloglog);
        htemp = hloglog;
    } else if (hloglog->version == STRUCT_VERSION) {
        htemp = hloglog;
    } else {
        elog(ERROR,"The version of the orginal struct %d is not supported by upgrade!",hloglog->version);
    }

    return htemp;
}

/* -------------------------------------------------------------------------
 * ---------------- OLD FUNCTIONS FOR BACKWARDS COMPATABILITY --------------
 * -------------------------------------------------------------------------*/
/* V1 functions */

/* Decompress header function */
static HLLCounter 
hll_decompress_V1(HLLCounter hloglog)
{
     /* make sure the data is compressed */
    if (hloglog->b > 0) {
        return hloglog;
    }

    if (hloglog->idx == -1){
        hloglog = hll_decompress_dense_V1(hloglog);
    } else {
        hloglog = hll_decompress_sparse_V1(hloglog);
    }

    return hloglog;
}

/* Decompresses sparse counters */
static HLLCounter 
hll_decompress_dense_V1(HLLCounter hloglog)
{
    char * dest;
    int m,i;
    HLLCounter htemp;
    /* reset b to positive value for calcs and to indicate data is
     * decompressed */
    hloglog->b = -1 * (hloglog->b);

    /* allocate and zero an array large enough to hold all the decompressed
     * bins */
    m = (int) pow(2,hloglog->b);
    dest = malloc(m);
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

/* Decompresses sparse counters. Which currently just means allocating more
 * memory and flipping the compression flag */
static HLLCounter 
hll_decompress_sparse_V1(HLLCounter hloglog)
{
    HLLCounter htemp;
    size_t length;

    /* reset b to positive value for calcs and to indicate data is
     * decompressed */
    hloglog->b = -1 * (hloglog->b);

    length = pow(2,(hloglog->b-2));

    htemp = palloc0(length);
    memcpy(htemp,hloglog,VARSIZE_ANY(hloglog));
    hloglog = htemp;

    SET_VARSIZE(hloglog,length);
    return hloglog;
}

