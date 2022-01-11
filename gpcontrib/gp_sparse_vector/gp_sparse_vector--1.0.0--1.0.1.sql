CREATE SCHEMA sparse_vector;

ALTER TYPE svec SET SCHEMA sparse_vector;

ALTER FUNCTION svec_in(cstring) 
	SET SCHEMA sparse_vector;

ALTER FUNCTION svec_out(sparse_vector.svec)
    SET SCHEMA sparse_vector;

ALTER FUNCTION svec_recv(internal)
    SET SCHEMA sparse_vector;

ALTER FUNCTION svec_send(sparse_vector.svec)
   SET SCHEMA sparse_vector;


-- Sparse Feature Vector (text processing) related functions
ALTER FUNCTION gp_extract_feature_histogram(text[], text[]) SET SCHEMA sparse_vector;

-- Basic floating point scalar operators MIN,MAX
ALTER FUNCTION dmin(float8,float8) SET SCHEMA sparse_vector;
ALTER FUNCTION dmax(float8,float8) SET SCHEMA sparse_vector;
-- Aggregate related functions
ALTER FUNCTION svec_count(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;

-- Scalar operator functions
ALTER FUNCTION svec_plus(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_minus(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION log(sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_div(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_mult(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_pow(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_eq(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_eq(float8[],float8[]) SET SCHEMA sparse_vector;
-- Permutation of float8[] and sparse_vector.svec for basic functions minus,plus,mult,div
ALTER FUNCTION float8arr_minus_float8arr(float8[],float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_minus_svec(float8[],sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_minus_float8arr(sparse_vector.svec,float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_plus_float8arr(float8[],float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_plus_svec(float8[],sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_plus_float8arr(sparse_vector.svec,float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_mult_float8arr(float8[],float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_mult_svec(float8[],sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_mult_float8arr(sparse_vector.svec,float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_div_float8arr(float8[],float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION float8arr_div_svec(float8[],sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_div_float8arr(sparse_vector.svec,float8[]) SET SCHEMA sparse_vector;

-- Vector operator functions
ALTER FUNCTION dot(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION dot(float8[],float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION dot(sparse_vector.svec,float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION dot(float8[],sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION l2norm(sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION l2norm(float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION l1norm(sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION l1norm(float8[]) SET SCHEMA sparse_vector;

ALTER FUNCTION unnest(sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION vec_pivot(sparse_vector.svec,float8) SET SCHEMA sparse_vector;
ALTER FUNCTION vec_sum(sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION vec_sum(float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION vec_median(float8[]) SET SCHEMA sparse_vector;
ALTER FUNCTION vec_median(sparse_vector.svec) SET SCHEMA sparse_vector;

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
ALTER FUNCTION svec_return_array(sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_concat(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_concat_replicate(int4,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION dimension(sparse_vector.svec) SET SCHEMA sparse_vector;


ALTER OPERATOR || (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR - (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR + (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR / (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR %*% (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR * (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;


ALTER OPERATOR ^ (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR = (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR %*% (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR %*% (float8[], sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR %*% (sparse_vector.svec, float8[])
	SET SCHEMA sparse_vector;


ALTER OPERATOR - (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR + (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR * (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR / (float8[], float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR - (float8[], sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR + (float8[], sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR * (float8[], sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR / (float8[], sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR - (sparse_vector.svec, float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR + (sparse_vector.svec, float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR * (sparse_vector.svec, float8[])
	SET SCHEMA sparse_vector;

ALTER OPERATOR / (sparse_vector.svec, float8[])
	SET SCHEMA sparse_vector;

-- CAST no schema issue
-- CREATE CAST (int2 AS sparse_vector.svec) WITH FUNCTION svec_cast_int2(int2) AS IMPLICIT;
-- CREATE CAST (integer AS sparse_vector.svec) WITH FUNCTION svec_cast_int4(integer) AS IMPLICIT;
-- CREATE CAST (bigint AS sparse_vector.svec) WITH FUNCTION svec_cast_int8(bigint) AS IMPLICIT;
-- CREATE CAST (float4 AS sparse_vector.svec) WITH FUNCTION svec_cast_float4(float4) AS IMPLICIT;
-- CREATE CAST (float8 AS sparse_vector.svec) WITH FUNCTION svec_cast_float8(float8) AS IMPLICIT;
-- CREATE CAST (numeric AS sparse_vector.svec) WITH FUNCTION svec_cast_numeric(numeric) AS IMPLICIT;

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

-- CREATE CAST (sparse_vector.svec AS float8[]) WITH FUNCTION svec_return_array(sparse_vector.svec) AS IMPLICIT;
-- CREATE CAST (float8[] AS sparse_vector.svec) WITH FUNCTION svec_cast_float8arr(float8[]) AS IMPLICIT;

ALTER OPERATOR = (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER AGGREGATE sum (sparse_vector.svec) SET SCHEMA sparse_vector;

-- Aggregate that provides a tally of nonzero entries in a list of vectors
ALTER AGGREGATE count_vec (sparse_vector.svec) SET SCHEMA sparse_vector;

ALTER AGGREGATE vec_count_nonzero (sparse_vector.svec) SET SCHEMA sparse_vector;

ALTER AGGREGATE array_agg (float8) SET SCHEMA sparse_vector;

ALTER AGGREGATE median_inmemory (float8) SET SCHEMA sparse_vector;

-- Comparisons based on L2 Norm
ALTER FUNCTION svec_l2_lt(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_le(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_eq(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_ne(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_gt(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_ge(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;
ALTER FUNCTION svec_l2_cmp(sparse_vector.svec,sparse_vector.svec) SET SCHEMA sparse_vector;

ALTER OPERATOR < (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR <= (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR <> (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR == (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR >= (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR > (sparse_vector.svec, sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR *|| (int4, sparse_vector.svec)
	SET SCHEMA sparse_vector;

ALTER OPERATOR CLASS svec_l2_ops USING btree SET SCHEMA sparse_vector;

