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

#include "utils/hyperloglog.h"
#include "utils/encoding.h"
#include "utils/hyperloglog_counter.h"

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
hyperloglog_get_estimate(HLLCounter hyperloglog)
{
	double estimate;

	/* unpack if needed */
	hyperloglog = hll_unpack(hyperloglog);

	estimate = hll_estimate(hyperloglog);

	/* return the updated bytea */
	return estimate;
}

HLLCounter
hyperloglog_merge(HLLCounter counter1, HLLCounter counter2)
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
hyperloglog_init_default()
{
	HLLCounter hyperloglog;

	hyperloglog = hll_create(DEFAULT_NDISTINCT, DEFAULT_ERROR, PACKED);

	return hyperloglog;
}


int
hyperloglog_size_default()
{
	return hll_get_size(DEFAULT_NDISTINCT, DEFAULT_ERROR);
}

int
hyperloglog_length(HLLCounter hyperloglog)
{
	return VARSIZE_ANY(hyperloglog);
}

void
hyperloglog_reset(HLLCounter hyperloglog)
{
	hll_reset_internal(hyperloglog);
	return;
}


HLLCounter
hyperloglog_comp(HLLCounter hyperloglog)
{
	if (hyperloglog == NULL)
	{
		return NULL;
	}

	hyperloglog = hll_compress(hyperloglog);

	return hyperloglog;
}

HLLCounter
hyperloglog_decomp(HLLCounter hyperloglog)
{
	if (hyperloglog == NULL)
	{
		return NULL;
	}

	if (hyperloglog-> b < 0 && hyperloglog->format == PACKED )
	{
		hyperloglog = hll_decompress(hyperloglog);
	}

	return hyperloglog;
}
