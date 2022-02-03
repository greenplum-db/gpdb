/*
 * gpcontrib/gp_array_agg/gp_array_agg.c
 *
 * Copyright (c) 2021-Present VMware, Inc. or its affiliates.
 *
 ******************************************************************************
  This file contains routines that can be bound to a Postgres backend and
  called by the backend in the process of processing queries.  The calling
  format for these routines is dictated by Postgres architecture.
******************************************************************************/

#include <string.h>
#include "postgres.h"

#include "fmgr.h"
#include "libpq/pqformat.h"		/* needed for send/recv functions */
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/datum.h"
#include "catalog/pg_type.h"


PG_MODULE_MAGIC;


/*
 * SerialIOData
 *		Used for caching element-type data in array_agg_serialize
 */
typedef struct SerialIOData
{
    FmgrInfo	typsend;
} SerialIOData;

/*
 * DeserialIOData
 *		Used for caching element-type data in array_agg_deserialize
 */
typedef struct DeserialIOData
{
    FmgrInfo	typreceive;
    Oid			typioparam;
} DeserialIOData;


/* Note : this is same as upstream initArrayResult, but allows the initial
 * size  of the allocated arrays to be specified.
 *
 * initArrayResultWithSize - initialize an empty ArrayBuildState
 *
 *	element_type is the array element type (must be a valid array element type)
 *	rcontext is where to keep working state
 *	subcontext is a flag determining whether to use a separate memory context
 *
 * Note: there are two common schemes for using accumArrayResult().
 * In the older scheme, you start with a NULL ArrayBuildState pointer, and
 * call accumArrayResult once per element.  In this scheme you end up with
 * a NULL pointer if there were no elements, which you need to special-case.
 * In the newer scheme, call initArrayResult and then call accumArrayResult
 * once per element.  In this scheme you always end with a non-NULL pointer
 * that you can pass to makeArrayResult; you get an empty array if there
 * were no elements.  This is preferred if an empty array is what you want.
 *
 * It's possible to choose whether to create a separate memory context for the
 * array build state, or whether to allocate it directly within rcontext.
 *
 * When there are many concurrent small states (e.g. array_agg() using hash
 * aggregation of many small groups), using a separate memory context for each
 * one may result in severe memory bloat. In such cases, use the same memory
 * context to initialize all such array build states, and pass
 * subcontext=false.
 *
 * In cases when the array build states have different lifetimes, using a
 * single memory context is impractical. Instead, pass subcontext=true so that
 * the array build states can be freed individually.
 */
static ArrayBuildState *
initArrayResultWithSize(Oid element_type, MemoryContext rcontext, bool subcontext, int initsize)
{
    ArrayBuildState *astate;
    MemoryContext arr_context = rcontext;

	/* Make a temporary context */
    if (subcontext)
        arr_context = AllocSetContextCreate(rcontext,
                                            "accumArrayResult",
                                            ALLOCSET_DEFAULT_SIZES);

    astate = (ArrayBuildState *)
            MemoryContextAlloc(arr_context, sizeof(ArrayBuildState));
    astate->mcontext = arr_context;
	astate->alen = initsize;
    astate->dvalues = (Datum *)
            MemoryContextAlloc(arr_context, astate->alen * sizeof(Datum));
    astate->dnulls = (bool *)
            MemoryContextAlloc(arr_context, astate->alen * sizeof(bool));
    astate->nelems = 0;
    astate->element_type = element_type;
    get_typlenbyvalalign(element_type,
                         &astate->typlen,
                         &astate->typbyval,
                         &astate->typalign);

    return astate;
}

/*
 * Since we use V1 function calling convention, all these functions have
 * the same signature as far as C is concerned.  We provide these prototypes
 * just to forestall warnings when compiled with gcc -Wmissing-prototypes.
 */
PG_FUNCTION_INFO_V1(array_agg_combine);
PG_FUNCTION_INFO_V1(array_agg_serialize);
PG_FUNCTION_INFO_V1(array_agg_deserialize);

Datum array_agg_combine(PG_FUNCTION_ARGS);
Datum array_agg_serialize(PG_FUNCTION_ARGS);
Datum array_agg_deserialize(PG_FUNCTION_ARGS);

Datum
array_agg_combine(PG_FUNCTION_ARGS)
{
    ArrayBuildState *state1;
    ArrayBuildState *state2;
    MemoryContext agg_context;
    MemoryContext old_context;
    int			i;

    if (!AggCheckCallContext(fcinfo, &agg_context))
        elog(ERROR, "aggregate function called in non-aggregate context");

    state1 = PG_ARGISNULL(0) ? NULL : (ArrayBuildState *) PG_GETARG_POINTER(0);
    state2 = PG_ARGISNULL(1) ? NULL : (ArrayBuildState *) PG_GETARG_POINTER(1);

    if (state2 == NULL)
    {
        /*
         * NULL state2 is easy, just return state1, which we know is already
         * in the agg_context
         */
        if (state1 == NULL)
            PG_RETURN_NULL();
        PG_RETURN_POINTER(state1);
    }

    if (state1 == NULL)
    {
        /* We must copy state2's data into the agg_context */
        state1 = initArrayResultWithSize(state2->element_type, agg_context,
                                         false, state2->alen);

        old_context = MemoryContextSwitchTo(agg_context);

        for (i = 0; i < state2->nelems; i++)
        {
            if (!state2->dnulls[i])
                state1->dvalues[i] = datumCopy(state2->dvalues[i],
                                               state1->typbyval,
                                               state1->typlen);
            else
                state1->dvalues[i] = (Datum) 0;
        }

        MemoryContextSwitchTo(old_context);

        memcpy(state1->dnulls, state2->dnulls, sizeof(bool) * state2->nelems);

        state1->nelems = state2->nelems;

        PG_RETURN_POINTER(state1);
    }
    else if (state2->nelems > 0)
    {
        /* We only need to combine the two states if state2 has any elements */
        int			reqsize = state1->nelems + state2->nelems;
        MemoryContext oldContext = MemoryContextSwitchTo(state1->mcontext);

        Assert(state1->element_type == state2->element_type);

        /* Enlarge state1 arrays if needed */
        if (state1->alen < reqsize)
        {
            /* Use a power of 2 size rather than allocating just reqsize */
            while (state1->alen < reqsize)
                state1->alen *= 2;

            state1->dvalues = (Datum *) repalloc(state1->dvalues,
                                                 state1->alen * sizeof(Datum));
            state1->dnulls = (bool *) repalloc(state1->dnulls,
                                               state1->alen * sizeof(bool));
        }

        /* Copy in the state2 elements to the end of the state1 arrays */
        for (i = 0; i < state2->nelems; i++)
        {
            if (!state2->dnulls[i])
                state1->dvalues[i + state1->nelems] =
                        datumCopy(state2->dvalues[i],
                                  state1->typbyval,
                                  state1->typlen);
            else
                state1->dvalues[i + state1->nelems] = (Datum) 0;
        }

        memcpy(&state1->dnulls[state1->nelems], state2->dnulls,
               sizeof(bool) * state2->nelems);

        state1->nelems = reqsize;

        MemoryContextSwitchTo(oldContext);
    }

    PG_RETURN_POINTER(state1);
}

/*
 * array_agg_serialize
 *		Serialize ArrayBuildState into bytea.
 */
Datum
array_agg_serialize(PG_FUNCTION_ARGS)
{
    ArrayBuildState *state;
    StringInfoData buf;
    bytea	   *result;

    /* cannot be called directly because of internal-type argument */
    Assert(AggCheckCallContext(fcinfo, NULL));

    state = (ArrayBuildState *) PG_GETARG_POINTER(0);

    pq_begintypsend(&buf);

    /*
     * element_type. Putting this first is more convenient in deserialization
     */
    pq_sendint(&buf, state->element_type, sizeof(Oid));

    /*
     * nelems -- send first so we know how large to make the dvalues and
     * dnulls array during deserialization.
     */
    pq_sendint64(&buf, state->nelems);

    /* alen can be decided during deserialization */

    /* typlen */
    pq_sendint(&buf, state->typlen, sizeof(int16));

    /* typbyval */
    pq_sendbyte(&buf, state->typbyval);

    /* typalign */
    pq_sendbyte(&buf, state->typalign);

    /* dnulls */
    pq_sendbytes(&buf, (char *) state->dnulls, sizeof(bool) * state->nelems);

    /*
     * dvalues.  By agreement with array_agg_deserialize, when the element
     * type is byval, we just transmit the Datum array as-is, including any
     * null elements.  For by-ref types, we must invoke the element type's
     * send function, and we skip null elements (which is why the nulls flags
     * must be sent first).
     */
    if (state->typbyval)
        pq_sendbytes(&buf, (char *) state->dvalues,
                     sizeof(Datum) * state->nelems);
    else
    {
        SerialIOData *iodata;
        int			i;

        /* Avoid repeat catalog lookups for typsend function */
        iodata = (SerialIOData *) fcinfo->flinfo->fn_extra;
        if (iodata == NULL)
        {
            Oid			typsend;
            bool		typisvarlena;

            iodata = (SerialIOData *)
                    MemoryContextAlloc(fcinfo->flinfo->fn_mcxt,
                                       sizeof(SerialIOData));
            getTypeBinaryOutputInfo(state->element_type, &typsend,
                                    &typisvarlena);
            fmgr_info_cxt(typsend, &iodata->typsend,
                          fcinfo->flinfo->fn_mcxt);
            fcinfo->flinfo->fn_extra = (void *) iodata;
        }

        for (i = 0; i < state->nelems; i++)
        {
            bytea	   *outputbytes;

            if (state->dnulls[i])
                continue;
            outputbytes = SendFunctionCall(&iodata->typsend,
                                           state->dvalues[i]);
            pq_sendint(&buf, VARSIZE(outputbytes) - VARHDRSZ, sizeof(int32));
            pq_sendbytes(&buf, VARDATA(outputbytes),
                         VARSIZE(outputbytes) - VARHDRSZ);
        }
    }

    result = pq_endtypsend(&buf);

    PG_RETURN_BYTEA_P(result);
}

Datum
array_agg_deserialize(PG_FUNCTION_ARGS)
{
    bytea	   *sstate;
    ArrayBuildState *result;
    StringInfoData buf;
    Oid			element_type;
    int64		nelems;
    const char *temp;

    if (!AggCheckCallContext(fcinfo, NULL))
        elog(ERROR, "aggregate function called in non-aggregate context");

    sstate = PG_GETARG_BYTEA_PP(0);

    /*
     * Copy the bytea into a StringInfo so that we can "receive" it using the
     * standard recv-function infrastructure.
     */
    initStringInfo(&buf);
    appendBinaryStringInfo(&buf,
                           VARDATA_ANY(sstate), VARSIZE_ANY_EXHDR(sstate));

    /* element_type */
    element_type = pq_getmsgint(&buf, sizeof(Oid));

    /* nelems */
    nelems = pq_getmsgint64(&buf);

    /* Create output ArrayBuildState with the needed number of elements */
    result = initArrayResultWithSize(element_type, CurrentMemoryContext,
                                     false, nelems);
    result->nelems = nelems;

    /* typlen */
    result->typlen = pq_getmsgint(&buf, sizeof(int16));

    /* typbyval */
    result->typbyval = pq_getmsgbyte(&buf);

    /* typalign */
    result->typalign = pq_getmsgbyte(&buf);

    /* dnulls */
    temp = pq_getmsgbytes(&buf, sizeof(bool) * nelems);
    memcpy(result->dnulls, temp, sizeof(bool) * nelems);

    /* dvalues --- see comment in array_agg_serialize */
    if (result->typbyval)
    {
        temp = pq_getmsgbytes(&buf, sizeof(Datum) * nelems);
        memcpy(result->dvalues, temp, sizeof(Datum) * nelems);
    }
    else
    {
        DeserialIOData *iodata;
        int			i;

        /* Avoid repeat catalog lookups for typreceive function */
        iodata = (DeserialIOData *) fcinfo->flinfo->fn_extra;
        if (iodata == NULL)
        {
            Oid			typreceive;

            iodata = (DeserialIOData *)
                    MemoryContextAlloc(fcinfo->flinfo->fn_mcxt,
                                       sizeof(DeserialIOData));
            getTypeBinaryInputInfo(element_type, &typreceive,
                                   &iodata->typioparam);
            fmgr_info_cxt(typreceive, &iodata->typreceive,
                          fcinfo->flinfo->fn_mcxt);
            fcinfo->flinfo->fn_extra = (void *) iodata;
        }

        for (i = 0; i < nelems; i++)
        {
            int			itemlen;
            StringInfoData elem_buf;
            char		csave;

            if (result->dnulls[i])
            {
                result->dvalues[i] = (Datum) 0;
                continue;
            }

            itemlen = pq_getmsgint(&buf, 4);
            if (itemlen < 0 || itemlen > (buf.len - buf.cursor))
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                                errmsg("insufficient data left in message")));

            /*
             * Rather than copying data around, we just set up a phony
             * StringInfo pointing to the correct portion of the input buffer.
             * We assume we can scribble on the input buffer so as to maintain
             * the convention that StringInfos have a trailing null.
             */
            elem_buf.data = &buf.data[buf.cursor];
            elem_buf.maxlen = itemlen + 1;
            elem_buf.len = itemlen;
            elem_buf.cursor = 0;

            buf.cursor += itemlen;

            csave = buf.data[buf.cursor];
            buf.data[buf.cursor] = '\0';

            /* Now call the element's receiveproc */
            result->dvalues[i] = ReceiveFunctionCall(&iodata->typreceive,
                                                     &elem_buf,
                                                     iodata->typioparam,
                                                     -1);

            buf.data[buf.cursor] = csave;
        }
    }

    pq_getmsgend(&buf);
    pfree(buf.data);

    PG_RETURN_POINTER(result);
}
