/*-------------------------------------------------------------------------
 *
 * interpolate.c
 *
 * Portions Copyright (c) 2012, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/adt/interpolate.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "catalog/pg_type.h"
#include "utils/date.h"
#include "utils/interpolate.h"
#include "utils/numeric.h"
#include "utils/timestamp.h"

/* 
 * linterp_abscissa
 *
 * Common code that checks arguments.  The result is a floating point value
 * representing what fraction of the distance x lies along the interval from
 * x0 to x1.  It can be negative or greater than one (extrapolation) though
 * this isn't the intended use.  If x0 == x1, then the fraction is not
 * determined and the function returns 0 and sets *notnull false.  In all
 * other cases (except error exits) *notnull is set to true.  An additional
 * flag indicates whether the abscissa value is equal to the lower boundary
 * value.
 */
static float8
linterp_abscissa(PG_FUNCTION_ARGS, bool *p_eq_bounds, bool *p_eq_abscissas)
{
	Oid         x_type;
	Oid         x0_type;
	Oid         x1_type;
	Oid			y0_type;
	Oid			y1_type;
	float8		p = 0;
	bool		eq_bounds = false;
	bool		eq_abscissas = false;
	
	/* The abscissa (x) arguments are nominally declared anyelement.
	 * All the type checking is up to us.  We insist that the types
	 * are exactly alike.  Explicit casts may be needed.
	 */
	x_type = get_fn_expr_argtype(fcinfo->flinfo, 0);
	x0_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
	x1_type = get_fn_expr_argtype(fcinfo->flinfo, 3);

	if (!OidIsValid(x_type)||!OidIsValid(x0_type)||!OidIsValid(x1_type))
	{
		elog(ERROR, "could not determine argument data types");
	}
	
	if ( x_type!=x0_type || x_type!=x1_type )
	{
		elog(ERROR, "abscissa types unequal");
	}
	
	/* The ordinate (y) arguments are specifically declared in the SQL
	 * function declaration.  Here we just check and insist they are
	 * identical.
	 */
	y0_type = get_fn_expr_argtype(fcinfo->flinfo, 2);
	y1_type = get_fn_expr_argtype(fcinfo->flinfo, 4);

	if ( y0_type !=  y1_type )
	{
		elog(ERROR, "mismatched ordinate types");
	}

	switch (x_type)
	{
	case INT8OID:
		{
			float8 x = (float8)PG_GETARG_INT64(0);
			float8 x0 = (float8)PG_GETARG_INT64(1);
			float8 x1 = (float8)PG_GETARG_INT64(3);
			if ( x1 == x0 )
			{
				eq_bounds = true;
				eq_abscissas = ( x == x0 );
			}
			else
				p = (x-x0)/(x1-x0);
		}
		break;
	case INT4OID:
		{
			float8 x = (float8)PG_GETARG_INT32(0);
			float8 x0 = (float8)PG_GETARG_INT32(1);
			float8 x1 = (float8)PG_GETARG_INT32(3);
			if ( x1 == x0 )
			{
				eq_bounds = true;
				eq_abscissas = ( x == x0 );
			}
			else
				p = (x-x0)/(x1-x0);
		}
		break;
	case INT2OID:
		{
			float8 x = (float8)PG_GETARG_INT16(0);
			float8 x0 = (float8)PG_GETARG_INT16(1);
			float8 x1 = (float8)PG_GETARG_INT16(3);
			
			if ( x1 == x0 )
			{
				eq_bounds = true;
				eq_abscissas = ( x == x0 );
			}
			else
				p = (x-x0)/(x1-x0);
		}
		break;
	case FLOAT4OID:
		{
			float8 x = (float8)PG_GETARG_FLOAT4(0);
			float8 x0 = (float8)PG_GETARG_FLOAT4(1);
			float8 x1 = (float8)PG_GETARG_FLOAT4(3);
			
			if ( x1 == x0 )
			{
				eq_bounds = true;
				eq_abscissas = ( x == x0 );
			}
			else
				p = (x-x0)/(x1-x0);
		}
		break;
	case FLOAT8OID:
		{
			float8 x = PG_GETARG_FLOAT8(0);
			float8 x0 = PG_GETARG_FLOAT8(1);
			float8 x1 = PG_GETARG_FLOAT8(3);
			
			if ( x1 == x0 )
			{
				eq_bounds = true;
				eq_abscissas = ( x == x0 );
			}
			else
				p = (x-x0)/(x1-x0);
		}
		break;
	case DATEOID:
		{
			DateADT x = PG_GETARG_DATEADT(0);
			DateADT x0 = PG_GETARG_DATEADT(1);
			DateADT x1 = PG_GETARG_DATEADT(3);
			int32 x_x0 = date_diff(x, x0);
			int32 x1_x0 = date_diff(x1, x0);
			
			if ( x1 == x0 )
			{
				eq_bounds = true;
				eq_abscissas = ( x_x0 == 0 );
			}
			else
				p = ((float8)x_x0)/((float8)x1_x0);
		}
		break;
	case TIMEOID:
		{
			TimeADT x = PG_GETARG_TIMEADT(0);
			TimeADT x0 = PG_GETARG_TIMEADT(1);
			TimeADT x1 = PG_GETARG_TIMEADT(3);
			
			p = time_li_fraction(x, x0, x1, &eq_bounds, &eq_abscissas);
		}
		break;
	case TIMESTAMPOID:
		{
			Timestamp x = PG_GETARG_TIMESTAMP(0);
			Timestamp x0 = PG_GETARG_TIMESTAMP(1);
			Timestamp x1 = PG_GETARG_TIMESTAMP(3);
			
			p = timestamp_li_fraction(x, x0, x1, &eq_bounds, &eq_abscissas);
		}
		break;
	case TIMESTAMPTZOID:
		{
			TimestampTz x = PG_GETARG_TIMESTAMPTZ(0);
			TimestampTz x0 = PG_GETARG_TIMESTAMPTZ(1);
			TimestampTz x1 = PG_GETARG_TIMESTAMPTZ(3);
			
			p = timestamptz_li_fraction(x, x0, x1, &eq_bounds, &eq_abscissas);
		}
		break;
	case INTERVALOID:
		{
			Interval * x = PG_GETARG_INTERVAL_P(0);
			Interval * x0 = PG_GETARG_INTERVAL_P(1);
			Interval * x1 = PG_GETARG_INTERVAL_P(3);
			
			p = interval_li_fraction(x, x0, x1, &eq_bounds, &eq_abscissas);
		}
		break;
	case NUMERICOID:    
		{
			Numeric x = PG_GETARG_NUMERIC(0);
			Numeric x0 = PG_GETARG_NUMERIC(1);
			Numeric x1 = PG_GETARG_NUMERIC(3);
			
			p = numeric_li_fraction(x, x0, x1, &eq_bounds, &eq_abscissas);
		}
		break;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("abscissa type not supported")));
	}

	if ( p_eq_bounds )
		*p_eq_bounds = eq_bounds;
	
	if ( p_eq_abscissas )
		*p_eq_abscissas = eq_abscissas;
	
	return p;
}




/*
 * The anticipated declaration is as follows:
 *
    CREATE FUNCTION linear_interpolate(
            x anyelement,
            x0 anyelement,
            y0 T,
            x1 anyelement,
            y1 T
            ) 
        RETURNS T
        LANGUAGE internal IMMUTABLE STRICT;
        AS 'linterp_T'; 
 * 
 * We check this in the common code included in each linear 
 * interpolation function.  The common code eliminates duplication 
 * and deals with multiple data types (under the anyelement
 * declaration) to avoid the need to define a different function for 
 * each abscissa type.  The ordinate-type specific linear interpolation
 * functions follow.  Each is of the form
 *
	PG_FUNCTION_INFO_V1(linterp_T);
	Datum
	linterp_T(PG_FUNCTION_ARGS)
	{
		float8		p;
		T			result;
		
		// Common 
		p = linterp_abscissa(fcinfo, ...); // declared in PG_FUNCTION_ARGS
		
		// Ordinate type specific code
	
		PG_RETURN_T(result);
	}
 *
 * The Python script src/include/catalog/li_extras.py generates
 * declarations appropriate for src/include/catalog/pg_proc.sql
 * and the upgrade script upg2_catupgrade_XXX.sql as well as human-
 * readable and terse regression tests for make installcheck.
 */
 
Datum
linterp_int64(PG_FUNCTION_ARGS)
{
	float8 y0;
	float8 y1;
	float8 p;
	float8 r;
	int64 result;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	y0 = (float8)PG_GETARG_INT64(2);
	y1 = (float8)PG_GETARG_INT64(4);
	
	if ( eq_bounds )
	{
		if ( eq_abscissas && y0 == y1 )
			r = y0;
		else 
			PG_RETURN_NULL();
	}
	else 
	{
		r = round(y0+p*(y1-y0));
		if ( r < LONG_MIN || r > LONG_MAX )
			ereport(ERROR,
					(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					 errmsg("value \"%f\" is out of range for type bigint", r)));
	}

	result = (int64)r;

	PG_RETURN_INT64(result);
}

Datum
linterp_int32(PG_FUNCTION_ARGS)
{
	float8 y0;
	float8 y1;
	float8 p;
	float8 r;
	int32 result;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	y0 = (float8)PG_GETARG_INT32(2);
	y1 = (float8)PG_GETARG_INT32(4);
	
	if ( eq_bounds )
	{
		if ( eq_abscissas && y0 == y1 )
			r = y0;
		else 
			PG_RETURN_NULL();
	}
	else 
	{
		r = round(y0+p*(y1-y0));
		if ( r < INT_MIN || r > INT_MAX )
			ereport(ERROR,
					(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					 errmsg("value \"%f\" is out of range for type integer", r)));
	}
	
	result = (int32)r;
	PG_RETURN_INT32(result);
}

Datum
linterp_int16(PG_FUNCTION_ARGS)
{
	float8 y0;
	float8 y1;
	float8 p;
	float8 r;
	int16 result;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	y0 = (float8)PG_GETARG_INT16(2);
	y1 = (float8)PG_GETARG_INT16(4);
	
	if ( eq_bounds )
	{
		if ( eq_abscissas && y0 == y1 )
			r = y0;
		else 
			PG_RETURN_NULL();
	}
	else 
	{
		r = round(y0+p*(y1-y0));
		if ( r < SHRT_MIN || r > SHRT_MAX )
			ereport(ERROR,
					(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					 errmsg("value \"%f\" is out of range for type smallint", r)));
	}	
	result = (int16)r;
	PG_RETURN_INT16(result);
}

Datum
linterp_float8(PG_FUNCTION_ARGS)
{
	float8 y0;
	float8 y1;
	float8 p;
	float8 result;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	y0 = (float8)PG_GETARG_FLOAT8(2);
	y1 = (float8)PG_GETARG_FLOAT8(4);
	
	if ( eq_bounds )
	{
		if ( eq_abscissas && y0 == y1 )
			result = y0;
		else 
			PG_RETURN_NULL();
	}
	else 
	{
		result = (float8)(y0+p*(y1-y0));
	}
	PG_RETURN_FLOAT8(result);
}

Datum
linterp_float4(PG_FUNCTION_ARGS)
{
	float8 y0;
	float8 y1;
	float8 p;
	float4 result;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	y0 = (float8)PG_GETARG_FLOAT4(2);
	y1 = (float8)PG_GETARG_FLOAT4(4);
	
	if ( eq_bounds )
	{
		if ( eq_abscissas && y0 == y1 )
			result = y0;
		else 
			PG_RETURN_NULL();
	}
	else 
	{
		result = (float4)(y0+p*(y1-y0));
	}
	
	PG_RETURN_FLOAT4(result);
}

Datum
linterp_DateADT(PG_FUNCTION_ARGS)
{
	DateADT y0;
	DateADT y1;
	int32 dy;
	float8 p;
	DateADT result;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	y0 = PG_GETARG_DATEADT(2);
	y1 = PG_GETARG_DATEADT(4);
	dy = date_diff(y1, y0);
	
	if ( eq_bounds )
	{
		if ( eq_abscissas && dy == 0 )
			result = y0;
		else 
			PG_RETURN_NULL();
	}
	else 
	{
		result = date_pl_days(y0, (int32) (p * (float8) dy));
	}

	PG_RETURN_DATEADT(result);
}


Datum
linterp_TimeADT(PG_FUNCTION_ARGS)
{
	TimeADT	y0;
	TimeADT	y1;
	float8	p;
	TimeADT	result = 0;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	y0 = PG_GETARG_TIMEADT(2);
	y1 = PG_GETARG_TIMEADT(4);
	
	if ( eq_bounds )
	{
		if ( eq_abscissas && (y0 == y1) )
			result = y0;
		else
			PG_RETURN_NULL();
	}
	else 
	{
		result = time_li_value(p, y0, y1);
	}

	PG_RETURN_TIMEADT(result);
}

Datum
linterp_Timestamp(PG_FUNCTION_ARGS)
{
	float8	p;
	Timestamp y0 = PG_GETARG_TIMESTAMP(2);
	Timestamp y1 = PG_GETARG_TIMESTAMP(4);
	Timestamp result = 0;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	if ( eq_bounds )
	{
		if ( eq_abscissas && timestamp_cmp_internal(y0,y1) == 0 )
			result = y0;
		else
			PG_RETURN_NULL();
	}
	else 
	{
		result = timestamp_li_value(p, y0, y1);
	}

	PG_RETURN_TIMESTAMP(result);
}

Datum
linterp_TimestampTz(PG_FUNCTION_ARGS)
{
	float8	p;
	TimestampTz	y0 = PG_GETARG_TIMESTAMP(2);
	TimestampTz	y1 = PG_GETARG_TIMESTAMP(4);
	TimestampTz	result = 0;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	if ( eq_bounds )
	{
		/* Internally, TSTZ is like TS, UTC is stored. */
		if ( eq_abscissas && timestamp_cmp_internal(y0,y1) == 0 )
			result = y0;
		else
			PG_RETURN_NULL();
	}
	else 
	{
		result = timestamptz_li_value(p, y0, y1);
	}
	
	PG_RETURN_TIMESTAMPTZ(result);
}

Datum
linterp_Interval(PG_FUNCTION_ARGS)
{
	float8	p;
	Interval *y0 = PG_GETARG_INTERVAL_P(2);
	Interval *y1 = PG_GETARG_INTERVAL_P(4);
	Interval *result = NULL;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	if ( eq_bounds )
	{
		if ( eq_abscissas && interval_cmp_internal(y0, y1) == 0 )
			result = y0;
		else
			PG_RETURN_NULL();
	}
	else 
	{
		result = interval_li_value(p, y0, y1);
	}
	
	PG_RETURN_INTERVAL_P(result);
}

Datum
linterp_Numeric(PG_FUNCTION_ARGS)
{
	float8 p;
	Numeric	y0 = PG_GETARG_NUMERIC(2);
	Numeric	y1 = PG_GETARG_NUMERIC(4);
	Numeric	result = NULL;
	bool eq_bounds = false;
	bool eq_abscissas = false;
	
	/* Common */
	p = linterp_abscissa(fcinfo, &eq_bounds, &eq_abscissas);
	
	/* Ordinate type specific code*/
	if ( eq_bounds )
	{
		if ( eq_abscissas && cmp_numerics(y0,y1) == 0 )
			result = y0;
		else
			PG_RETURN_NULL();
	}
	else 
	{
		result = numeric_li_value(p, y0, y1);
	}
	
	PG_RETURN_NUMERIC(result);
}


