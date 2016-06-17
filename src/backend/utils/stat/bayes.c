#include "postgres.h"
#include "funcapi.h"

#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "executor/executor.h"  /* for GetAttributeByNum() */


/* What a nb_classify_type looks like */
typedef struct {
	ArrayType *classes;    /* Anyarray */
	ArrayType *accum;      /* float8[] */
	ArrayType *total;      /* int8[] */
} nb_classify_state;


/* 
 * Helper function used to extract interesting information from
 * the state tuple.
 */
static void get_nb_state(HeapTupleHeader tuple, 
						 nb_classify_state *state,
						 int nclasses);


/*
 * nb_classify_accum - Naive Bayesian Classification Accumulator
 */
Datum 
nb_classify_accum(PG_FUNCTION_ARGS)
{
	nb_classify_state  state;
	TupleDesc          resultDesc;
	int                i;
	int                nclasses;
	ArrayType         *classes;          /* arg[1] */
	int64              attr_count;       /* arg[2] */
	ArrayType         *class_count;      /* arg[3] */
	ArrayType         *class_total;      /* arg[4] */
	HeapTuple          result;
	Datum              resultDatum[3];
	bool               resultNull[3];
	bool               skip;
	int64             *class_data;
	int64             *total_data;
	float8            *prior_data;
	int64             *stotal_data;

	/* Check input parameters */
	if (PG_NARGS() != 5)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("nb_classify_accum called with %d arguments", 
						PG_NARGS())));
	}

	/* Skip rows with NULLs */
	if (PG_ARGISNULL(1) || PG_ARGISNULL(3) || PG_ARGISNULL(4))
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		PG_RETURN_DATUM(PG_GETARG_DATUM(0));
	}
	classes     = PG_GETARG_ARRAYTYPE_P(1);
	attr_count  = PG_ARGISNULL(2) ? 0 : PG_GETARG_INT64(2);
	class_count = PG_GETARG_ARRAYTYPE_P(3);
	class_total = PG_GETARG_ARRAYTYPE_P(4);

	if (ARR_NDIM(classes) != 1 || 
		ARR_NDIM(class_count) != 1 || 
		ARR_NDIM(class_total) != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("nb_classify cannot accumulate multidimensional arrays")));
	}

	/* All three arrays must be equal cardinality */
	nclasses = ARR_DIMS(classes)[0];
	if (ARR_DIMS(class_count)[0] != nclasses ||
		ARR_DIMS(class_total)[0] != nclasses)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("nb_classify: non-conformable arrays")));
	}
	
	class_data = (int64*) ARR_DATA_PTR(class_count);
	total_data = (int64*) ARR_DATA_PTR(class_total);

	/* It is an error for class_data, total_data, or attr_count to be a 
	   negative number */
	if (attr_count < 0) 
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("nb_classify: attr_count value must be >= 0")));

	}
	skip = false;
	for (i = 0; i < nclasses; i++) 
	{
		if (class_data[i] < 0) 
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("nb_classify: class_data values must be >= 0")));
		}
		if (total_data[i] < 0) 
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("nb_classify: total_data values must be >= 0")));
		}
		if (class_data[i] > total_data[i]) 
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("nb_classify: class_data values must be <= total_data")));
		}

		/* 
		 * If we do not have an adjustment value and any class has a zero value
		 * then we must skip this row, otherwise we will try to calculate ln(0)
		 */
		if (attr_count == 0 && class_data[i] == 0) 
		{
			skip = true;
		}
	}
	if (skip)
	{
		PG_RETURN_DATUM(PG_GETARG_DATUM(0));			
	}



	/* Get/create the accumulation state */
	if (!PG_ARGISNULL(0))
	{
		HeapTupleHeader tup;
		tup = (fcinfo->context && IsA(fcinfo->context, AggState))
			? PG_GETARG_HEAPTUPLEHEADER(0)
			: PG_GETARG_HEAPTUPLEHEADER_COPY(0);

		get_nb_state(tup, &state, nclasses);
	}
	else
	{
		/* Construct the state arrays */
		int     size;

		/* allocate memory and copy the classes array */
		size = VARSIZE(classes);
		state.classes = (ArrayType *) palloc(size);
		SET_VARSIZE(state.classes, size);
		memcpy(state.classes, classes, size);

		/* allocate memory and construct the accumulator array */
		size = ARR_OVERHEAD_NONULLS(1) + nclasses * sizeof(float8);
		state.accum = (ArrayType *) palloc(size);
		SET_VARSIZE(state.accum, size);
		state.accum->ndim          = 1;
		state.accum->dataoffset    = 0;
		ARR_ELEMTYPE(state.accum)  = FLOAT8OID;
		ARR_DIMS(state.accum)[0]   = nclasses;
		ARR_LBOUND(state.accum)[0] = 1;
		prior_data = (float8*) ARR_DATA_PTR(state.accum);
		for (i = 0; i < nclasses; i++)
			prior_data[i] = 0;

		/* allocate memory and construct the total array */
		size = ARR_OVERHEAD_NONULLS(1) + nclasses * sizeof(int64);
		state.total = (ArrayType *) palloc(size);
		SET_VARSIZE(state.total, size);
		state.total->ndim          = 1;
		state.total->dataoffset    = 0;
		ARR_ELEMTYPE(state.total)  = INT8OID;
		ARR_DIMS(state.total)[0]   = nclasses;
		ARR_LBOUND(state.total)[0] = 1;
		stotal_data = (int64*) ARR_DATA_PTR(state.total);
		for (i = 0; i < nclasses; i++)
			stotal_data[i] = 0;
	}

	/* Adjust the prior based on the current input row */
	prior_data = (float8*) ARR_DATA_PTR(state.accum);
	stotal_data = (int64*) ARR_DATA_PTR(state.total);
	for (i = 0; i < nclasses; i++)
	{
		/*
		 * Calculate the accumulation value for the classifier
		 *
		 * The logical calculation is: 
		 *     product((class[i]+1)/(total_data[i]+attr_count))
		 *
		 * Instead of this calculation we calculate:
		 *     sum(ln((class[i]+1)/(total_data[i]+attr_count)))
		 *
		 * The reason for this is to increase the numerical stability of
		 * the algorithm.
		 *
		 * Since the ln(0) is undefined we want to increment the count 
		 * for all classes. 
		 *
		 * This get's a bit more complicated for the denominator which
		 * needs to know how many values there are for this attribute
		 * so that we keep the total probability for the attribute = 1.
		 * To handle this the aggregation function should be passed the
		 * number of distinct values for the aggregate it is computing.
		 *
		 * If for some reason this value is not present, or < 1 then we
		 * just switch to non-adjusted calculations.  In this case we 
		 * will simply skip over any row that has a 0 count on any 
		 * class_data index. (handled above)
		 */
		if (attr_count > 1)
			prior_data[i] += log((class_data[i]+1)/(float8)(total_data[i] + attr_count));
		else
			prior_data[i] += log(class_data[i]/(float8)total_data[i]);

		/* 
		 * The total array should be constant throughout, but if not it should
		 * reflect the maximum values encountered 
		 */
		if (total_data[i] > stotal_data[i])
			stotal_data[i] = total_data[i];
	}


	/* Construct the return tuple */
	if (get_call_result_type(fcinfo, NULL, &resultDesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");
	BlessTupleDesc(resultDesc); 
	
	resultDatum[0] = PointerGetDatum(state.classes);
	resultDatum[1] = PointerGetDatum(state.accum);
	resultDatum[2] = PointerGetDatum(state.total);
	resultNull[0]  = false;
	resultNull[1]  = false;
	resultNull[2]  = false;

	result = heap_form_tuple(resultDesc, resultDatum, resultNull);
	PG_RETURN_DATUM(HeapTupleGetDatum(result));
}

Datum 
nb_classify_combine(PG_FUNCTION_ARGS)
{
	HeapTupleHeader    tup;
	TupleDesc          resultDesc;
	HeapTuple          result;
	Datum              resultDatum[3];
	bool               resultNull[3];
	nb_classify_state  state[2];
	float8            *prior_data1;
	float8            *prior_data2;
	int                nclasses;
	int                i;

	/* Need to be called with two arguments */
	if (PG_NARGS() != 2)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("nb_classify_final called with %d arguments", 
						PG_NARGS())));
	}
	if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(1))
		PG_RETURN_DATUM(PG_GETARG_DATUM(0));
	if (PG_ARGISNULL(0))
		PG_RETURN_DATUM(PG_GETARG_DATUM(1));


	tup = (fcinfo->context && IsA(fcinfo->context, AggState))
		? PG_GETARG_HEAPTUPLEHEADER(0)
		: PG_GETARG_HEAPTUPLEHEADER_COPY(0);
	get_nb_state(tup, &state[0], 0);
	nclasses = ARR_DIMS(state[0].classes)[0];

	get_nb_state(PG_GETARG_HEAPTUPLEHEADER(1), &state[1], nclasses);

	/* The the prior with maximum likelyhood */
	prior_data1 = (float8*) ARR_DATA_PTR(state[0].accum);
	prior_data2 = (float8*) ARR_DATA_PTR(state[1].accum);
	nclasses = ARR_DIMS(state[0].classes)[0];
	for (i = 0; i < nclasses; i++)
		prior_data1[i] += prior_data2[i];

	/* Construct the return tuple */
	if (get_call_result_type(fcinfo, NULL, &resultDesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");
	BlessTupleDesc(resultDesc);
	
	resultDatum[0] = PointerGetDatum(state[0].classes);
	resultDatum[1] = PointerGetDatum(state[0].accum);
	resultDatum[2] = PointerGetDatum(state[0].total);
	resultNull[0]  = false;
	resultNull[1]  = false;
	resultNull[2]  = false;

	result = heap_form_tuple(resultDesc, resultDatum, resultNull);
	PG_RETURN_DATUM(HeapTupleGetDatum(result));
}

Datum 
nb_classify_final(PG_FUNCTION_ARGS)
{
	nb_classify_state  state;
	int64             *total_data;
	float8             total;
	float8            *prior_data;
	char              *class_data;
	int                i, maxi, nclasses;

	if (PG_NARGS() != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("nb_classify_final called with %d arguments", 
						PG_NARGS())));
	}
	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	get_nb_state(PG_GETARG_HEAPTUPLEHEADER_COPY(0), &state, 0);
	
	/* The the prior with maximum likelyhood */
	prior_data = (float8*) ARR_DATA_PTR(state.accum);
	total_data = (int64*) ARR_DATA_PTR(state.total);
	nclasses = ARR_DIMS(state.classes)[0];
	for (total = i = 0; i < nclasses; i++)
		total += total_data[i];
	for (i = 0; i < nclasses; i++)
		prior_data[i] += log(total_data[i]/total);
	for (maxi = 0, i = 1; i < nclasses; i++)
		if (prior_data[i] > prior_data[maxi])
			maxi = i;
	
	/* 
	 * Looking up values in text arrays in a pain.  You must start at the
	 * beginning and walk adding the length of the current element until
	 * you get to the index you are interested in.
	 */
	/*
	 * GPDB_83_MERGE_FIXME: is it guaranteed that there are no short varlenas
	 * in the array?
	 */
	class_data = (char*) ARR_DATA_PTR(state.classes);
	for (i = 0; i < maxi; i++)
	{
		class_data += VARSIZE(class_data);
		class_data = (char*) att_align_nominal(class_data, 'i');
	}
	PG_RETURN_TEXT_P(class_data);
}

/* 
 * nb_classify_probabilities - calculate naive bayes probabilty vector
 * 
 * Similar to nb_classify_final, except the return value is the vector of
 * probabilities for each class rather than the text value of the most
 * likely class.
 */
Datum 
nb_classify_probabilities(PG_FUNCTION_ARGS)
{
	nb_classify_state  state;
	int64             *total_data;
	float8             maxprior, normalize;
	float8            *prior_data;
	int                i, nclasses;

	if (PG_NARGS() != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("nb_classify_probabilities called with %d arguments", 
						PG_NARGS())));
	}
	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	get_nb_state(PG_GETARG_HEAPTUPLEHEADER_COPY(0), &state, 0);
	
	/* The the prior with maximum likelyhood */
	prior_data = (float8*) ARR_DATA_PTR(state.accum);
	total_data = (int64*) ARR_DATA_PTR(state.total);
	nclasses = ARR_DIMS(state.classes)[0];

	/* Adjust prior to account for percentages of total distributions */
	for (i = 0; i < nclasses; i++)
		prior_data[i] += log(total_data[i]);

	/* Calculate max prior to improve numeric stability */
	maxprior = prior_data[0];
	for (i = 1; i < nclasses; i++)
		maxprior = Max(maxprior, prior_data[i]);

	/* Convert log values into true values */
	for (normalize = i = 0; i < nclasses; i++)
	{
		prior_data[i] = exp(prior_data[i] - maxprior);
		normalize += prior_data[i];
	}

	/* Normalize results */
	for (i = 0; i < nclasses; i++)
		prior_data[i] /= normalize;

	PG_RETURN_ARRAYTYPE_P(state.accum);
}


/* 
 * Helper function used to extract interesting information from
 * the state tuple.
 */
static void get_nb_state(HeapTupleHeader tuple, 
						 nb_classify_state *state,
						 int nclasses)
{
	Datum class_datum, accum_datum, total_datum;
	bool  isnull[3];

	/* 
	 * When called correctly nb_classify should never fill in the state
	 * tuple with any invalid values, but if a user is calling the
	 * functions by hand for some reason then something may be funny 
	 */
	class_datum = GetAttributeByNum(tuple, 1, &isnull[0]);
	accum_datum = GetAttributeByNum(tuple, 2, &isnull[1]);
	total_datum = GetAttributeByNum(tuple, 3, &isnull[2]);
	if (isnull[0] || isnull[1] || isnull[2])
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("nb_classify: invalid accumulation state")));
	}
	state->classes = DatumGetArrayTypeP(class_datum);
	state->accum = DatumGetArrayTypeP(accum_datum);
	state->total = DatumGetArrayTypeP(total_datum);

	/* All must have the correct dimensionality */
	if (ARR_NDIM(state->classes) != 1 || 
		ARR_NDIM(state->accum) != 1 ||
		ARR_NDIM(state->total) != 1 ||
		ARR_DIMS(state->accum)[0] != ARR_DIMS(state->classes)[0] ||
		ARR_DIMS(state->classes)[0] != ARR_DIMS(state->classes)[0])
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("nb_classify: invalid accumulation state")));
	}

	/* Check that lengths matchup with what was expected */
	if (nclasses > 0 && ARR_DIMS(state->classes)[0] != nclasses) 
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("nb_classify: mismatched inter-row input lengths")));
	}
}
