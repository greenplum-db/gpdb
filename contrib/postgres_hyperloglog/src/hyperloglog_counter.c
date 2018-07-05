/* File contains functions that interact directly with the postgres api */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/lsyscache.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"

#include "utils/hyperloglog/hyperloglog.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/* PG_GETARG macros for HLLCounter's that does version checking */
#define PG_GETARG_HLL_P(n) pg_check_hll_version((HLLCounter) PG_GETARG_BYTEA_P(n))
#define PG_GETARG_HLL_P_COPY(n) pg_check_hll_version((HLLCounter) PG_GETARG_BYTEA_P_COPY(n))

/* shoot for 2^64 distinct items and 0.8125% error rate by default */
#define DEFAULT_NDISTINCT   1ULL << 63 
#define DEFAULT_ERROR       0.008125

/* Use the PG_FUNCTION_INFO_V! macro to pass functions to postgres */
PG_FUNCTION_INFO_V1(hyperloglog_add_item_agg_default);

PG_FUNCTION_INFO_V1(hyperloglog_merge);
PG_FUNCTION_INFO_V1(hyperloglog_get_estimate);

PG_FUNCTION_INFO_V1(hyperloglog_init_default);
PG_FUNCTION_INFO_V1(hyperloglog_length);

PG_FUNCTION_INFO_V1(hyperloglog_in);
PG_FUNCTION_INFO_V1(hyperloglog_out);

PG_FUNCTION_INFO_V1(hyperloglog_comp);

/* ------------- function declarations for local functions --------------- */
Datum hyperloglog_add_item_agg_default(PG_FUNCTION_ARGS);

Datum hyperloglog_get_estimate(PG_FUNCTION_ARGS);
Datum hyperloglog_merge(PG_FUNCTION_ARGS);

Datum hyperloglog_init_default(PG_FUNCTION_ARGS);
Datum hyperloglog_length(PG_FUNCTION_ARGS);

Datum hyperloglog_in(PG_FUNCTION_ARGS);
Datum hyperloglog_out(PG_FUNCTION_ARGS);

Datum hyperloglog_comp(PG_FUNCTION_ARGS);

static HLLCounter pg_check_hll_version(HLLCounter hloglog);

/* ---------------------- function definitions --------------------------- */
static HLLCounter 
pg_check_hll_version(HLLCounter hloglog)
{
    if (hloglog->version != STRUCT_VERSION){
        elog(ERROR,"ERROR: The stored counter is version %u while the library is version %u. Please change library version or use upgrade function to upgrade the counter",hloglog->version,STRUCT_VERSION);
    }
    return hloglog;
}

Datum
hyperloglog_add_item_agg_default(PG_FUNCTION_ARGS)
{

    HLLCounter hyperloglog;

    /* info for anyelement */
    Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
    Datum       element = PG_GETARG_DATUM(1);
    int16       typlen;
    bool        typbyval;
    char        typalign;
    
    /* Create a new estimator (with default error rate and ndistinct) or reuse
     * the existing one. Return null if both counter and element args are null.
     * This prevents excess empty counter creation */
    if (PG_ARGISNULL(0) && PG_ARGISNULL(1)){
        PG_RETURN_NULL();
    } else if (PG_ARGISNULL(0)) {
    hyperloglog = hll_create(DEFAULT_NDISTINCT, DEFAULT_ERROR, PACKED);
    } else {
        hyperloglog = PG_GETARG_HLL_P(0);
    }

    /* add the item to the estimator (skip NULLs) */
    if (! PG_ARGISNULL(1)) {

        /* TODO The requests for type info shouldn't be a problem (thanks to
         * lsyscache), but if it turns out to have a noticeable impact it's
         * possible to cache that between the calls (in the estimator).
         *
         * I have noticed no measurable effect from either option. */

        /* get type information for the second parameter (anyelement item) */
        get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

	hyperloglog = hyperloglog_add_item(hyperloglog, element, typlen, typbyval, typalign);
    }

    /* return the updated bytea */
    PG_RETURN_BYTEA_P(hyperloglog);

}

Datum
hyperloglog_merge(PG_FUNCTION_ARGS)
{

    HLLCounter counter1;
    HLLCounter counter2;

    if (PG_ARGISNULL(0) && PG_ARGISNULL(1)){
        /* if both counters are null return null */
        PG_RETURN_NULL();

    } else if (PG_ARGISNULL(0)) {
        /* if first counter is null just copy the second estimator into the
         * first one */
        counter1 = PG_GETARG_HLL_P(1);

    } else if (PG_ARGISNULL(1)) {
        /* if second counter is null just return the the first estimator */
        counter1 = PG_GETARG_HLL_P(0);

    } else {
	/* ok, we already have the estimator - merge the second one into it */
	counter1 = PG_GETARG_HLL_P_COPY(0);
	counter2 = PG_GETARG_HLL_P_COPY(1);

	counter1 = hyperloglog_merge_counters(counter1, counter2);
    }

    /* return the updated bytea */
    PG_RETURN_BYTEA_P(counter1);


}


Datum
hyperloglog_get_estimate(PG_FUNCTION_ARGS)
{

    double estimate;
    HLLCounter hyperloglog = PG_GETARG_HLL_P_COPY(0);
    
    estimate = hyperloglog_estimate(hyperloglog);

    /* return the updated bytea */
    PG_RETURN_FLOAT8(estimate);

}

Datum
hyperloglog_init_default(PG_FUNCTION_ARGS)
{
      HLLCounter hyperloglog;

      hyperloglog = hyperloglog_init_def();

      PG_RETURN_BYTEA_P(hyperloglog);
}

Datum
hyperloglog_length(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT32(hyperloglog_len(PG_GETARG_HLL_P(0)));
}

Datum
hyperloglog_out(PG_FUNCTION_ARGS)
{
    int16   datalen, resultlen, res;
    char     *result;
    bytea    *data = PG_GETARG_BYTEA_P(0);

    datalen = VARSIZE_ANY_EXHDR(data);
    resultlen = b64_enc_len(VARDATA_ANY(data), datalen);
    result = palloc(resultlen + 1);
    res = hll_b64_encode(VARDATA_ANY(data),datalen, result);
    
    /* Make this FATAL 'cause we've trodden on memory ... */
    if (res > resultlen)
        elog(FATAL, "overflow - encode estimate too small");

    result[res] = '\0';

    PG_RETURN_CSTRING(result);
}

Datum
hyperloglog_in(PG_FUNCTION_ARGS)
{
    bytea      *result;
    char       *data = PG_GETARG_CSTRING(0);
    int16      datalen, resultlen, res;

    datalen = strlen(data);
    resultlen = b64_dec_len(data,datalen);
    result = palloc(VARHDRSZ + resultlen);
    res = hll_b64_decode(data, datalen, VARDATA(result));

    /* Make this FATAL 'cause we've trodden on memory ... */
    if (res > resultlen)
        elog(FATAL, "overflow - decode estimate too small");

    SET_VARSIZE(result, VARHDRSZ + res);

    PG_RETURN_BYTEA_P(result);
    
}

Datum
hyperloglog_comp(PG_FUNCTION_ARGS)
{
    HLLCounter hyperloglog;

    if (PG_ARGISNULL(0) ){
        PG_RETURN_NULL();
    }

    hyperloglog =  PG_GETARG_HLL_P_COPY(0);

    hyperloglog = hll_compress(hyperloglog);

    PG_RETURN_BYTEA_P(hyperloglog);
}

