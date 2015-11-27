#include "postgres.h"
#include "funcapi.h"

#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

#include "pinv.h"
#include "student.h"

#include <dlfcn.h>
#include <math.h>
#include <string.h>

/*
 * The size of the transition state array (in number of elements, not in
 * bytes), given the number of independent variables (len).
 * The number of independent variables is assumed to be less than 2^32 (i.e., of
 * type uint32 or smaller). Thus,
 * 
 *     STATE_LEN(len) <= 4 + (2^32 - 1) + (2^32 - 1)^2
 *                     = 2^64 - 2^33 + 2^32 + 4 < 2^64,
 *
 * i.e., we do not have to worry about long int overflows.
 * HOWEVER: Caution is required as the size in bytes could still cause an
 * long int overflow!
 */
#define STATE_LEN(len) (4ULL + len + ((uint64) len) * len)

/*
 * The following condition will not cause an long int overflow and is equivalent
 * (note: despite the long int division!) to:
 *
 *     state_len * sizeof(float8) + ARR_OVERHEAD_NONULLS(1) <= MaxAllocSize
 *
 * It is a precondition before palloc'ing the transition state array.
 */
#define IS_FEASIBLE_STATE_LEN(state_len) ( \
	(MaxAllocSize - ARR_OVERHEAD_NONULLS(1)) / sizeof(float8) >= state_len)


/* Transition state for multi-linear regression functions and new arguments. */
typedef struct {
	ArrayType   *stateAsArray;
	float8      *len;
	float8      *count;
	float8      *sumy;
	float8      *sumy2;
	float8      *Xty;
	float8      *XtX;

	float8      newY;
	ArrayType   *newXAsArray;
	float8      *newX;
} MRegrAccumArgs;

/* Final state for multi-linear regression functions. */
typedef struct {
	uint32              len;		/* scalar:               len(X[]) */
	unsigned long long  count;		/* scalar:               count(*) */
	float8              sumy;		/* scalar:               sum(y)   */
	float8              sumy2;		/* scalar:               sum(y*y) */
	float8              *Xty;		/* vector[count]:        sum(X'[] * y) */
	float8              *XtX;		/* matrix[count][count]: sum(X'[] * X[]) */
} MRegrState;


/* Prototypes for static functions */

static bool float8_mregr_accum_get_args(FunctionCallInfo fcinfo,
										MRegrAccumArgs *outArgs);
static bool float8_mregr_get_state(FunctionCallInfo fcinfo,
								   MRegrState *outState);
static void float8_mregr_compute(MRegrState	*inState,
								 ArrayType	**outCoef,
								 float8		*outR2,
								 ArrayType	**outTStats,
								 ArrayType	**outPValues);

static void symmetricMatrixTimesVector(uint32 inSize, float8 *inMatrix,
	float8 *inVector, float8 *outResult);
static double dotProduct(uint32 inSize, float8 *inVec1, float8 *inVec2);
	
static bool
float8_mregr_accum_get_args(FunctionCallInfo fcinfo,
							MRegrAccumArgs *outArgs)
{
	float8      *stateData;
	uint32      len, i;
	uint64      statelen;
	
	/* We should be strict, but it doesn't hurt to be paranoid */
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1) || PG_ARGISNULL(2))
		return false;
	
	outArgs->stateAsArray = PG_GETARG_ARRAYTYPE_P(0);	
   	outArgs->newY = PG_GETARG_FLOAT8(1);
	outArgs->newXAsArray = PG_GETARG_ARRAYTYPE_P(2);
	outArgs->newX  = (float8*) ARR_DATA_PTR(outArgs->newXAsArray);
	
	/* Ensure that both arrays are single dimensional float8[] arrays */
	if (ARR_NULLBITMAP(outArgs->stateAsArray) ||
		ARR_NDIM(outArgs->stateAsArray) != 1 || 
		ARR_ELEMTYPE(outArgs->stateAsArray) != FLOAT8OID ||
		ARR_NDIM(outArgs->newXAsArray) != 1 ||
		ARR_ELEMTYPE(outArgs->newXAsArray) != FLOAT8OID)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	/* Only callable as a transition function */
	if (!(fcinfo->context && IsA(fcinfo->context, AggState)))
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" not called from aggregate",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	/* newXAsArray with nulls will be ignored */
	if (ARR_NULLBITMAP(outArgs->newXAsArray))
		return false;
	
	/* See MPP-14102. Avoid overflow while initializing len */
	if (ARR_DIMS(outArgs->newXAsArray)[0] > UINT32_MAX)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("number of independent variables cannot exceed %lu",
				 (unsigned long) UINT32_MAX)));
	len = ARR_DIMS(outArgs->newXAsArray)[0];
		
	/*
	 * See MPP-13580. At least on certain platforms and with certain versions,
	 * LAPACK will run into an infinite loop if pinv() is called for non-finite
	 * matrices. We extend the check also to the dependent variables.
	 */
	for (i = 0; i < len; i++)
		if (!isfinite(outArgs->newX[i]))
			ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("design matrix is not finite")));
	if (!isfinite(outArgs->newY))
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("dependent variables are not finite")));
	
	/*
	 * See MPP-14102. We want to avoid (a) long int overflows and (b) making
	 * oversized allocation requests.
	 * We could compute the maximum number of variables so that the transition-
	 * state length still fits into MaxAllocSize, but (assuming MaxAllocSize may
	 * change in the future) this calculation requires taking the root out of a
	 * 64-bit long int. Since there is no standard library function for that, and
	 * displaying this number of merely of theoretical interest (the actual
	 * limit is a lot lower), we simply report that the number of independent
	 * variables is too large.
	 * Precondition:
	 *     len < 2^32.
	 */
	statelen = STATE_LEN(len);
	if (!IS_FEASIBLE_STATE_LEN(statelen))
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("number of independent variables is too large")));

	/*
	 * If length(outArgs->stateAsArray) == 1 then it is an unitialized state.
	 * We extend as needed.
	 */
	if (ARR_DIMS(outArgs->stateAsArray)[0] == 1)
	{
		/*
		 * Precondition:
		 *     IS_FEASIBLE_STATE_LEN(statelen)
		 */
		Size size = statelen * sizeof(float8) + ARR_OVERHEAD_NONULLS(1);
		outArgs->stateAsArray = (ArrayType *) palloc(size);
		SET_VARSIZE(outArgs->stateAsArray, size);
		outArgs->stateAsArray->ndim = 1;
		outArgs->stateAsArray->dataoffset = 0;
		outArgs->stateAsArray->elemtype = FLOAT8OID;
		ARR_DIMS(outArgs->stateAsArray)[0] = statelen;
		ARR_LBOUND(outArgs->stateAsArray)[0] = 1;
		stateData = (float8*) ARR_DATA_PTR(outArgs->stateAsArray);
		memset(stateData, 0, statelen * sizeof(float8));
		stateData[0] = len;
	}
	
	/* 
	 * Contents of 'state' are as follows:
	 *   [0]     = len(X[])
	 *   [1]     = count
	 *   [2]     = sum(y)
	 *   [3]     = sum(y*y)
	 *   [4:N]   = sum(X'[] * y) 
	 *   [N+1:M] = sum(X[] * X'[])
	 *   N       = 3 + len(X)
	 *   M       = N + len(X)*len(X)
	 */
	outArgs->len = (float8*) ARR_DATA_PTR(outArgs->stateAsArray);
	outArgs->count = outArgs->len + 1;
	outArgs->sumy = outArgs->len + 2;
	outArgs->sumy2 = outArgs->len + 3;
	outArgs->Xty = outArgs->len + 4;
	outArgs->XtX = outArgs->len + 4 + len;
	
	/* It is an error if the number of indepent variables is not constant */
	if (*outArgs->len != len)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid)),
				 errdetail("The independent-variable array is not of constant width.")));
	}
	
	/* Something is seriously fishy if our state has the wrong length */
	if ((uint64) ARR_DIMS(outArgs->stateAsArray)[0] != statelen)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	}
	
	/* Okay... All's good now do the work */
	return true;
}


/* Transition function used by multi-linear regression aggregates. */
Datum
float8_mregr_accum(PG_FUNCTION_ARGS)
{
	bool            goodArguments;
	MRegrAccumArgs  args;
	uint32          len, i,j;

	goodArguments = float8_mregr_accum_get_args(fcinfo, &args);
	if (!goodArguments) {
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		else
			PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
	}

	len = (int) *args.len;
	(*args.count)++;
	*args.sumy += args.newY;
	*args.sumy2 += args.newY * args.newY;
	for (i = 0; i < len; i++)
		args.Xty[i] += args.newY * args.newX[i];
	
	/* Compute the matrix X[] * X'[] and add it in */
	for (i = 0; i < len; i++)
		for (j = 0; j < len; j++)
			/* Precondition:
			 *     len^2 - 1 <= STATE_LEN(len)
			 * and IS_FEASIBLE_STATE_LEN(STATE_LEN(len))
			 */
			args.XtX[(uint64) i * len + j] += args.newX[i] * args.newX[j];
	
	PG_RETURN_ARRAYTYPE_P(args.stateAsArray);
}


/*
 * Preliminary segment-level calculation function for multi-linear regression
 * aggregates.
 */
Datum
float8_mregr_combine(PG_FUNCTION_ARGS)
{
	ArrayType  *state1, *state2, *result;
	float8     *state1Data, *state2Data, *resultData;
	uint32     len;
	uint64     statelen, i;
	Size       size;
	
	/* We should be strict, but it doesn't hurt to be paranoid */
	if (PG_ARGISNULL(0))
	{
		if (PG_ARGISNULL(1))
			PG_RETURN_NULL();
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(1));
	}
	if (PG_ARGISNULL(1))
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
	
	state1 = PG_GETARG_ARRAYTYPE_P(0);	
	state2 = PG_GETARG_ARRAYTYPE_P(1);
	
	/* Ensure that both arrays are single dimensional float8[] arrays */
	if (ARR_NULLBITMAP(state1) || ARR_NULLBITMAP(state2) || 
		ARR_NDIM(state1) != 1 || ARR_NDIM(state2) != 1 || 
		ARR_ELEMTYPE(state1) != FLOAT8OID || ARR_ELEMTYPE(state2) != FLOAT8OID)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("preliminary segment-level calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	}
	
	/* 
	 * Remember that we initialized to {0}, so if either array is still at
	 * the initial value then just return the other one 
	 */
	if (ARR_DIMS(state1)[0] == 1)
		PG_RETURN_ARRAYTYPE_P(state2);
	if (ARR_DIMS(state2)[0] == 1)
		PG_RETURN_ARRAYTYPE_P(state1);
	
	state1Data = (float8*) ARR_DATA_PTR(state1);
	state2Data = (float8*) ARR_DATA_PTR(state2);
	
	if (ARR_DIMS(state1)[0] != ARR_DIMS(state2)[0] || 
		state1Data[0] != state2Data[0])
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("preliminary segment-level calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid)),
				 errdetail("The independent-variable array is not of constant width.")));
	}
	len = state1Data[0];
	statelen = STATE_LEN(len);
	
	/*
	 * Violation of any of the following conditions indicates bogus inputs.
	 */
	if (state1Data[0] > UINT32_MAX ||
		(uint64) ARR_DIMS(state1)[0] != statelen ||
		!IS_FEASIBLE_STATE_LEN(statelen))
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("preliminary segment-level calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	}
	
	/* Validations pass, allocate memory for result and do work */

	/*
	 * Precondition:
	 *     IS_FEASIBLE_STATE_LEN(statelen)
	 */
	size = statelen * sizeof(float8) + ARR_OVERHEAD_NONULLS(1);
	result = (ArrayType *) palloc(size);
	SET_VARSIZE(result, size);
	result->ndim = 1;
	result->dataoffset = 0;
	result->elemtype = FLOAT8OID;
	ARR_DIMS(result)[0] = statelen;
	ARR_LBOUND(result)[0] = 1;
	resultData = (float8*) ARR_DATA_PTR(result);
	memset(resultData, 0, statelen * sizeof(float8));
	
	/* 
	 * Contents of 'state' are as follows:
	 *   [0]     = len(X[])
	 *   [1]     = count
	 *   [2]     = sum(y)
	 *   [3]     = sum(y*y)
	 *   [4:N]   = sum(X'[] * y) 
	 *   [N+1:M] = sum(X[] * X'[])
	 *   N       = 3 + len(X)
	 *   M       = N + len(X)*len(X)
	 */
	resultData[0] = len;
	for (i = 1; i < statelen; i++)
		resultData[i] = state1Data[i] + state2Data[i];	
	PG_RETURN_ARRAYTYPE_P(result);
}


/*
 * Check that a valid state is passed to the aggregate's final function.
 * If we return false, the calling function should return NULL.
 */
static bool
float8_mregr_get_state(FunctionCallInfo fcinfo,
					   MRegrState *outState)
{
	ArrayType	*in;
	float8		*data;
	
	/* Input should be a single parameter, the aggregate state */
	if (PG_NARGS() != 1)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("final calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	if (PG_ARGISNULL(0))
		return false;
	
	/* Validate array type */
	in = PG_GETARG_ARRAYTYPE_P(0);
	if (ARR_ELEMTYPE(in) != FLOAT8OID || ARR_NDIM(in) != 1 || ARR_NULLBITMAP(in))
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("final calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	/* Validate the correct size input */
	if (ARR_DIMS(in)[0] < 2)
		return false;  /* no input */
	
	data = (float8*) ARR_DATA_PTR(in);
	outState->len    = (int) data[0];   /* scalar:           len(X[]) */
	if ((uint64) ARR_DIMS(in)[0] != 4ULL + outState->len + outState->len * outState->len) 
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("final calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	outState->count  = data[1];         /* scalar:           count(*) */
	outState->sumy   = data[2];         /* scalar:           sum(y)   */
	outState->sumy2  = data[3];         /* scalar:           sum(y*y) */
	outState->Xty    = &data[4];        /* vector:           X^t * y  */
	outState->XtX    = &data[4 + outState->len]; /* matrix:  X^t * X  */

	return true;
}

/*----------
 * Do the computations requested from final functions.
 *
 * Compute regression coefficients, coefficient of determination (R^2),
 * t-statistics, and p-values whenever the respective argument is non-NULL.
 * Since these functions share a lot of computation, they have been distilled
 * into this function.
 *
 * First, we compute the regression coefficients as:
 *   
 *   c = (X^T X)^+ * X^T * y = X^+ * y
 *
 * where:
 * 
 *   X^T = the transpose of X
 *   X^+ = the pseudo-inverse of X
 *
 * The identity X^+ = (X^T X)^+ X^T holds for all matrices X, a proof
 * can be found here:
 * http://en.wikipedia.org/wiki/Proofs_involving_the_Moore%2DPenrose_pseudoinverse
 *
 * Note that when the system X c = y is satisfiable (because (X|c) has rank at
 * most inState->len), then setting c = X^+ y means that |c|_2 <= |d|_2 for all
 * solutions d satisfying X d = y.
 * (See http://en.wikipedia.org/wiki/Moore%2DPenrose_pseudoinverse)
 *
 * Caveat: Explicitly computing (X^T X)^+ can become a significant source of
 * numerical rounding erros (see, e.g., 
 * http://en.wikipedia.org/wiki/Moore%2DPenrose_pseudoinverse#Construction
 * or http://www.mathworks.com/moler/leastsquares.pdf p.16).
 *----------
 */
static void
float8_mregr_compute(MRegrState	*inState,
					 ArrayType	**outCoef,
					 float8		*outR2,
					 ArrayType	**outTStats,
					 ArrayType	**outPValues)
{
	ArrayType   *coef_array, *stdErr_array, *tStats_array = NULL, *pValues_array = NULL;
	float8      ess = 0., /* explained sum of squares (regression sum of squares) */
	            tss = 0., /* total sum of squares */
	            rss, /* residual sum of squares */
	            r2,
	            variance;
	float8      *XtX_inv, *coef, *stdErr, *tStats = NULL, *pValues = NULL;
	uint32      i;
	
	/*
	 * Precondition: inState->len * inState->len * sizeof(float8) < STATE_LEN(inState->len)
	 *           and IS_FEASIBLE_STATE_LEN(STATE_LEN(inState->len))
	 */
	XtX_inv = palloc0((uint64) inState->len * inState->len * sizeof(float8));
	pinv(inState->len, inState->len, inState->XtX, XtX_inv);
	
	/*
	 * FIXME: Decide on whether we want to display an info message or rather
	 * provide a second function that tells how well the data is conditioned.
	 *
	 * Check if we should expect multicollineratiy. [MPP-13582]
	 *
	 * See:
	 * Lichtblau, Daniel and Weisstein, Eric W. "Condition Number."
	 * From MathWorld--A Wolfram Web Resource.
	 * http://mathworld.wolfram.com/ConditionNumber.html
	 *
	if (condNoOfXtX > 1000)
		ereport(INFO, 
		        (errmsg("matrix X^T X is ill-conditioned"),
		         errdetail("condition number = %f", condNoOfXtX),
		         errhint("This can indicate strong multicollinearity.")));
	 */
	
	/*
	 * We want to return a one-dimensional array (as opposed to a
	 * two-dimensional array).
	 *
	 * Note: Calling construct_array with NULL as first arguments is a Greenplum
	 * extension
	 */
	coef_array = construct_array(NULL, inState->len,
								 FLOAT8OID, sizeof(float8), true, 'd');
	coef = (float8 *) ARR_DATA_PTR(coef_array);
	symmetricMatrixTimesVector(inState->len, XtX_inv, inState->Xty, coef);

	if (outCoef)
		*outCoef = coef_array;
	
	if (outR2 || outTStats || outPValues)
	{	
		/*----------
		 * Computing the total sum of squares (tss) and the explained sum of squares (ess)
		 *
		 *   ess = y'X * c - sum(y)^2/n
		 *   tss = sum(y^2) - sum(y)^2/n
		 *   R^2 = ess/tss
		 *----------
		 */
		ess = dotProduct(inState->len, inState->Xty, coef)
			  - inState->sumy*inState->sumy/inState->count;
		tss = inState->sumy2 - inState->sumy * inState->sumy / inState->count;
		
		/*
		 * With infinite precision, the following checks are pointless. But due to
		 * floating-point arithmetic, this need not hold at this point.
		 * Without a formal proof convincing us of the contrary, we should
		 * anticipate that numerical peculiarities might occur.
		 */
		if (tss < 0)
			tss = 0;
		if (ess < 0)
			ess = 0;
		/*
		 * Since we know tss with greater accuracy than ess, we do the following
		 * sanity adjustment to ess:
		 */
		if (ess > tss)
			ess = tss;
	}
	
	if (outR2)
	{
		/*
		 * coefficient of determination
		 * If tss == 0, then the regression perfectly fits the data, so the
		 * coefficient of determination is 1.
		 */
		r2 = (tss == 0 ? 1 : ess / tss);
		*outR2 = r2;
	}
	
	if (outTStats || outPValues)
	{
		stdErr_array = construct_array(NULL, inState->len,
		                               FLOAT8OID, sizeof(float8), true, 'd');
		stdErr = (float8 *) ARR_DATA_PTR(stdErr_array);
		tStats_array = construct_array(NULL, inState->len,
		                               FLOAT8OID, sizeof(float8), true, 'd');
		tStats = (float8 *) ARR_DATA_PTR(tStats_array);
		pValues_array = construct_array(NULL, inState->len,
		                                FLOAT8OID, sizeof(float8), true, 'd');
		pValues = (float8 *) ARR_DATA_PTR(pValues_array);

		/*
		 * Computing t-statistics and p-values
		 *
		 * Total sum of squares (tss) = Residual Sum of sqaures (rss) +
		 * Explained Sum of Squares (ess) for linear regression.
		 * Proof: http://en.wikipedia.org/wiki/Sum_of_squares
		 */
		rss = tss - ess;
		
		/* Variance is also called the Mean Square Error */
		variance = rss / (inState->count - inState->len);
		
		/*
		 * The t-statistic for each coef[i] is coef[i] / stdErr[i]
		 * where stdErr[i] is the standard error of coef[ii], i.e.,
		 * the square root of the i'th diagonoal element of
		 * variance * (X^T X)^{-1}.
		 */
		for (i = 0; i < inState->len; i++) {
			/*
			 * In an abundance of caution, we see a tiny possibility that numerical
			 * instabilities in the pinv operation can lead to negative values on
			 * the main diagonal of even a SPD matrix
			 */
			if (XtX_inv[i * (inState->len + 1)] < 0) {
				stdErr[i] = 0;
			} else {
				stdErr[i] = sqrt( variance * XtX_inv[i * (inState->len + 1)] );
			}
			
			if (coef[i] == 0 && stdErr[i] == 0) {
				/*
				 * In this special case, 0/0 should be interpreted as 0:
				 * We know that 0 is the exact value for the coefficient, so
				 * the t-value should be 0 (corresponding to a p-value of 1)
				 */
				tStats[i] = 0;
			} else {
				/*
				 * If stdErr[i] == 0 then abs(tStats[i]) will be infinity, which is
				 * what we need.
				 */
				tStats[i] = coef[i] / stdErr[i];
			}
		}
	}
	
	if (outTStats)
		*outTStats = tStats_array;
	
	if (outPValues) {
		for (i = 0; i < inState->len; i++)
			if (inState->count <= inState->len) {
				/*
				 * This test is purely for detecting long int overflows because
				 * studentT_cdf expects an unsigned long int as first argument.
				 */
				pValues[i] = NAN;
			} else {
				pValues[i] = 2. * (1. - studentT_cdf(
						(uint64) (inState->count - inState->len), fabs( tStats[i] )
					));
			}
		
		*outPValues = pValues_array;
	}
}


/*
 * PostgreSQL final function for computing regression coefficients.
 *
 * This function is essentially a wrapper for float8_mregr_compute().
 */
Datum
float8_mregr_coef(PG_FUNCTION_ARGS)
{
	bool goodArguments;
	MRegrState state;
	ArrayType *coef;
	
	goodArguments = float8_mregr_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();
	
	float8_mregr_compute(&state, &coef /* coefficients */, NULL /* R2 */,
	                     NULL /* t-statistics */, NULL /* p-values */);
	
	PG_RETURN_ARRAYTYPE_P(coef);
}

/*
 * PostgreSQL final function for computing the coefficient of determination, $R^2$.
 *
 * This function is essentially a wrapper for float8_mregr_compute().
 */
Datum
float8_mregr_r2(PG_FUNCTION_ARGS)
{
	bool goodArguments;
	MRegrState state;
	float8 r2 = 0.0;
	
	goodArguments = float8_mregr_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();
	
	float8_mregr_compute(&state, NULL /* coefficients */, &r2,
	                     NULL /* t-statistics */, NULL /* p-values */);
	
	PG_RETURN_FLOAT8(r2);
}

/*
 * PostgreSQL final function for computing the vector of t-statistics, for every coefficient.
 *
 * This function is essentially a wrapper for float8_mregr_compute().
 */
Datum
float8_mregr_tstats(PG_FUNCTION_ARGS)
{
	bool goodArguments;
	MRegrState state;
	ArrayType *tstats;
	
	goodArguments = float8_mregr_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();
	
	float8_mregr_compute(&state, NULL /* coefficients */, NULL /* R2 */,
	                     &tstats, NULL /* p-values */);
	
	PG_RETURN_ARRAYTYPE_P(tstats);
}

/*
 * PostgreSQL final function for computing the vector of p-values, for every coefficient.
 *
 * This function is essentially a wrapper for float8_mregr_compute().
 */
Datum
float8_mregr_pvalues(PG_FUNCTION_ARGS)
{
	bool goodArguments;
	MRegrState state;
	ArrayType *pvalues;
	
	goodArguments = float8_mregr_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();
	
	float8_mregr_compute(&state, NULL /* coefficients */, NULL /* R2 */,
						 NULL /* t-statistics */, &pvalues);
	
	PG_RETURN_ARRAYTYPE_P(pvalues);
}

/*
 * Multiply symmetric matrix with vector
 */
static void
symmetricMatrixTimesVector(uint32 inSize, float8 *inMatrix,
	float8 *inVector, float8 *outResult)
{
	int			i,
				j;

	/*-----------
	 * Math recap:
	 *
	 *     | a b c |     | x |
	 * A = | p q r | B = | y |
	 *     | u v w |     | z |
	 *
	 *         | ax + by + cz |
	 * A * B = | px + qy + rz |
	 *         | ux + vy + wz |
	 *
	 *-----------
	 */
	for (i = 0; i < inSize; i++)
	{
		outResult[i] = 0;
		for (j = 0; j < inSize; j++)
			outResult[i] += inMatrix[i*inSize + j] * inVector[j];
	}
}

/* Dot product */
static float8
dotProduct(uint32 inSize, float8 *inVec1, float8 *inVec2)
{
	float8		result;
	int			i;

	result = 0;
	for (i = 0; i < inSize; i++)
		result += inVec1[i] * inVec2[i];

	return result;
}
