ALTER FUNCTION svec_in(cstring) 
	SET SCHEMA sparse_vector;

ALTER FUNCTION svec_out(svec)
    SET SCHEMA sparse_vector;

ALTER FUNCTION svec_recv(internal)
    SET SCHEMA sparse_vector;

ALTER FUNCTION svec_send(svec)
   SET SCHEMA sparse_vector;

ALTER TYPE svec SET SCHEMA sparse_vector;

-- Sparse Feature Vector (text processing) related functions
ALTER FUNCTION gp_extract_feature_histogram(text[], text[]) SET SCHEMA sparse_vector;

-- Basic floating point scalar operators MIN,MAX
ALTER FUNCTION dmin(float8,float8) SET SCHEMA sparse_vector;
ALTER FUNCTION dmax(float8,float8) SET SCHEMA sparse_vector;
-- Aggregate related functions
ALTER FUNCTION svec_count(svec,svec) SET SCHEMA sparse_vector;

-- Scalar operator functions
ALTER FUNCTION svec_plus(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_minus(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION log(svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_div(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_mult(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_pow(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_eq(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_eq(float8[],float8[]) SET SCHEMA sparse_vector;
-- Permutation of float8[] and svec for basic functions minus,plus,mult,div
ALTER FUNCTION float8arr_minus_float8arr(float8[],float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_minus_svec(float8[],svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_minus_float8arr(svec,float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_plus_float8arr(float8[],float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_plus_svec(float8[],svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_plus_float8arr(svec,float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_mult_float8arr(float8[],float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_mult_svec(float8[],svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_mult_float8arr(svec,float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_div_float8arr(float8[],float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_div_svec(float8[],svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_div_float8arr(svec,float8[]) SET SCHEMA sparse_vector;

-- Vector operator functions
ALTER FUNCTION dot(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION dot(float8[],float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION dot(svec,float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION dot(float8[],svec) SET SCHEMA sparse_vector;
ALTER FUNCTION l2norm(svec) SET SCHEMA sparse_vector;
ALTER FUNCTION l2norm(float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION l1norm(svec) SET SCHEMA sparse_vector;
ALTER FUNCTION l1norm(float8[]) SET SCHEMA sparse_vector;

ALTER FUNCTION unnest(svec) SET SCHEMA sparse_vector;
ALTER FUNCTION vec_pivot(svec,float8) SET SCHEMA sparse_vector;
ALTER FUNCTION vec_sum(svec) SET SCHEMA sparse_vector;
ALTER FUNCTION vec_sum(float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION vec_median(float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION vec_median(svec) SET SCHEMA sparse_vector;

-- Casts and transforms
ALTER FUNCTION svec_cast_int2(int2) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_cast_int4(int4) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_cast_int8(bigint) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_cast_float4(float4) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_cast_float8(float8) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_cast_numeric(numeric) SET SCHEMA sparse_vector;

ALTER FUNCTION float8arr_cast_int2(int2) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_cast_int4(int4) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_cast_int8(bigint) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_cast_float4(float4) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_cast_float8(float8) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_cast_numeric(numeric) SET SCHEMA sparse_vector;

ALTER FUNCTION svec_cast_float8arr(float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_return_array(svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_concat(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_concat_replicate(int4,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION dimension(svec) SET SCHEMA sparse_vector;


ALTER OPERATOR || (svec, svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR - (svec, svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR + (svec, svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR / (svec, svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR %*% (svec, svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR * (svec, svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR ^ (svec, svec)
	SET SCHEMA sparse_vector;



ALTER OPERATOR = (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR %*% (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR %*% (float8[], svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR %*% (svec, float8[])
	SET SCHEMA sparse_vector;


ALTER OPERATOR - (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR + (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR * (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR / (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR - (float8[], svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR + (float8[], svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR * (float8[], svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR / (float8[], svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR - (svec, float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR + (svec, float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR * (svec, float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR / (svec, float8[])
	SET SCHEMA sparse_vector;

-- CAST no schema issue
-- CREATE CAST (int2 AS svec) WITH FUNCTION svec_cast_int2(int2) AS IMPLICIT;
-- CREATE CAST (integer AS svec) WITH FUNCTION svec_cast_int4(integer) AS IMPLICIT;
-- CREATE CAST (bigint AS svec) WITH FUNCTION svec_cast_int8(bigint) AS IMPLICIT;
-- CREATE CAST (float4 AS svec) WITH FUNCTION svec_cast_float4(float4) AS IMPLICIT;
-- CREATE CAST (float8 AS svec) WITH FUNCTION svec_cast_float8(float8) AS IMPLICIT;
-- CREATE CAST (numeric AS svec) WITH FUNCTION svec_cast_numeric(numeric) AS IMPLICIT;

-- DROP CAST IF EXISTS (int2 AS float8[]) ;
-- DROP CAST IF EXISTS (integer AS float8[]) ;
-- DROP CAST IF EXISTS (bigint AS float8[]) ;
-- DROP CAST IF EXISTS (float4 AS float8[]) ;
-- DROP CAST IF EXISTS (float8 AS float8[]) ;
-- DROP CAST IF EXISTS (numeric AS float8[]) ;

-- ALTER CAST (int2 AS float8[]) WITH FUNCTION float8arr_cast_int2(int2) AS IMPLICIT;
-- ALTER CAST (integer AS float8[]) WITH FUNCTION float8arr_cast_int4(integer) AS IMPLICIT;
-- ALTER CAST (bigint AS float8[]) WITH FUNCTION float8arr_cast_int8(bigint) AS IMPLICIT;
-- ALTER CAST (float4 AS float8[]) WITH FUNCTION float8arr_cast_float4(float4) AS IMPLICIT;
-- ALTER CAST (float8 AS float8[]) WITH FUNCTION float8arr_cast_float8(float8) AS IMPLICIT;
-- ALTER CAST (numeric AS float8[]) WITH FUNCTION float8arr_cast_numeric(numeric) AS IMPLICIT;

-- CREATE CAST (svec AS float8[]) WITH FUNCTION svec_return_array(svec) AS IMPLICIT;
-- CREATE CAST (float8[] AS svec) WITH FUNCTION svec_cast_float8arr(float8[]) AS IMPLICIT;

ALTER OPERATOR = (svec, svec);
	SET SCHEMA sparse_vector;

ALTER AGGREGATE sum (svec) SET SCHEMA sparse_vector;

-- Aggregate that provides a tally of nonzero entries in a list of vectors
ALTER AGGREGATE count_vec (svec) SET SCHEMA sparse_vector;

ALTER AGGREGATE vec_count_nonzero (svec) SET SCHEMA sparse_vector;
);

ALTER AGGREGATE array_agg (float8) SET SCHEMA sparse_vector;

ALTER AGGREGATE median_inmemory (float8) SET SCHEMA sparse_vector;

-- Comparisons based on L2 Norm
ALTER FUNCTION svec_l2_lt(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_le(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_eq(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_ne(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_gt(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_ge(svec,svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_cmp(svec,svec) SET SCHEMA sparse_vector;

ALTER OPERATOR < (svec, svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR <= (svec, svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR <> (svec, svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR == (svec, svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR >= (svec, svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR > (svec, svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR *|| (svec, svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR CLASS svec_l2_ops SET SCHEMA sparse_vector;