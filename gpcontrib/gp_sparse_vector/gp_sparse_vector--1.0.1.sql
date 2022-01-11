-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_sparse_vector" to load this file. \quit

CREATE SCHEMA sparse_vector;

SET search_path TO sparse_vector;

DROP TYPE IF EXISTS svec CASCADE;
CREATE TYPE svec;

CREATE FUNCTION svec_in(cstring)
    RETURNS svec
    AS 'gp_svec.so'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION svec_out(svec)
    RETURNS cstring
    AS 'gp_svec.so'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION svec_recv(internal)
    RETURNS svec
    AS 'gp_svec.so'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION svec_send(svec)
    RETURNS bytea
    AS 'gp_svec.so'
    LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE svec (
	   internallength = VARIABLE, 
	   input = svec_in,
	   output = svec_out,
	   send = svec_send,
	   receive = svec_recv,
	   storage=EXTENDED,
	   alignment = double
);

-- Sparse Feature Vector (text processing) related functions
CREATE OR REPLACE FUNCTION gp_extract_feature_histogram(text[], text[]) RETURNS svec AS 'gp_svec.so', 'gp_extract_feature_histogram' LANGUAGE C IMMUTABLE; 

-- Basic floating point scalar operators MIN,MAX
CREATE OR REPLACE FUNCTION dmin(float8,float8) RETURNS float8 AS 'gp_svec.so', 'float8_min' LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION dmax(float8,float8) RETURNS float8 AS 'gp_svec.so', 'float8_max' LANGUAGE C IMMUTABLE; 

-- Aggregate related functions
CREATE OR REPLACE FUNCTION svec_count(svec,svec) RETURNS svec AS 'gp_svec.so', 'svec_count' STRICT LANGUAGE C IMMUTABLE; 

-- Scalar operator functions
CREATE OR REPLACE FUNCTION svec_plus(svec,svec) RETURNS svec AS 'gp_svec.so', 'svec_plus' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_minus(svec,svec) RETURNS svec AS 'gp_svec.so', 'svec_minus' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION log(svec) RETURNS svec AS 'gp_svec.so', 'svec_log' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_div(svec,svec) RETURNS svec AS 'gp_svec.so', 'svec_div' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_mult(svec,svec) RETURNS svec AS 'gp_svec.so', 'svec_mult' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_pow(svec,svec) RETURNS svec AS 'gp_svec.so', 'svec_pow' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_eq(svec,svec) RETURNS boolean AS 'gp_svec.so', 'svec_eq' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION float8arr_eq(float8[],float8[]) RETURNS boolean AS 'gp_svec.so', 'float8arr_equals' LANGUAGE C IMMUTABLE;
-- Permutation of float8[] and svec for basic functions minus,plus,mult,div
CREATE OR REPLACE FUNCTION float8arr_minus_float8arr(float8[],float8[]) RETURNS svec AS 'gp_svec.so', 'float8arr_minus_float8arr' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION float8arr_minus_svec(float8[],svec) RETURNS svec AS 'gp_svec.so', 'float8arr_minus_svec' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION svec_minus_float8arr(svec,float8[]) RETURNS svec AS 'gp_svec.so', 'svec_minus_float8arr' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION float8arr_plus_float8arr(float8[],float8[]) RETURNS svec AS 'gp_svec.so', 'float8arr_plus_float8arr' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION float8arr_plus_svec(float8[],svec) RETURNS svec AS 'gp_svec.so', 'float8arr_plus_svec' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION svec_plus_float8arr(svec,float8[]) RETURNS svec AS 'gp_svec.so', 'svec_plus_float8arr' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION float8arr_mult_float8arr(float8[],float8[]) RETURNS svec AS 'gp_svec.so', 'float8arr_mult_float8arr' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION float8arr_mult_svec(float8[],svec) RETURNS svec AS 'gp_svec.so', 'float8arr_mult_svec' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION svec_mult_float8arr(svec,float8[]) RETURNS svec AS 'gp_svec.so', 'svec_mult_float8arr' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION float8arr_div_float8arr(float8[],float8[]) RETURNS svec AS 'gp_svec.so', 'float8arr_div_float8arr' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION float8arr_div_svec(float8[],svec) RETURNS svec AS 'gp_svec.so', 'float8arr_div_svec' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION svec_div_float8arr(svec,float8[]) RETURNS svec AS 'gp_svec.so', 'svec_div_float8arr' LANGUAGE C IMMUTABLE;

-- Vector operator functions
CREATE OR REPLACE FUNCTION dot(svec,svec) RETURNS float8 AS 'gp_svec.so', 'svec_dot' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION dot(float8[],float8[]) RETURNS float8 AS 'gp_svec.so', 'float8arr_dot' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION dot(svec,float8[]) RETURNS float8 AS 'gp_svec.so', 'svec_dot_float8arr' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION dot(float8[],svec) RETURNS float8 AS 'gp_svec.so', 'float8arr_dot_svec' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION l2norm(svec) RETURNS float8 AS 'gp_svec.so', 'svec_l2norm' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION l2norm(float8[]) RETURNS float8 AS 'gp_svec.so', 'float8arr_l2norm' LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION l1norm(svec) RETURNS float8 AS 'gp_svec.so', 'svec_l1norm' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION l1norm(float8[]) RETURNS float8 AS 'gp_svec.so', 'float8arr_l1norm' STRICT LANGUAGE C IMMUTABLE; 

CREATE OR REPLACE FUNCTION unnest(svec) RETURNS setof float8  AS 'gp_svec.so', 'svec_unnest' LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION vec_pivot(svec,float8) RETURNS svec  AS 'gp_svec.so', 'svec_pivot' LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION vec_sum(svec) RETURNS float8 AS 'gp_svec.so', 'svec_summate' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION vec_sum(float8[]) RETURNS float8 AS 'gp_svec.so', 'float8arr_summate' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION vec_median(float8[]) RETURNS float8 AS 'gp_svec.so', 'float8arr_median' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION vec_median(svec) RETURNS float8 AS 'gp_svec.so', 'svec_median' STRICT LANGUAGE C IMMUTABLE; 

-- Casts and transforms
CREATE OR REPLACE FUNCTION svec_cast_int2(int2) RETURNS svec AS 'gp_svec.so', 'svec_cast_int2' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_cast_int4(int4) RETURNS svec AS 'gp_svec.so', 'svec_cast_int4' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_cast_int8(bigint) RETURNS svec AS 'gp_svec.so', 'svec_cast_int8' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_cast_float4(float4) RETURNS svec AS 'gp_svec.so', 'svec_cast_float4' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_cast_float8(float8) RETURNS svec AS 'gp_svec.so', 'svec_cast_float8' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_cast_numeric(numeric) RETURNS svec AS 'gp_svec.so', 'svec_cast_numeric' STRICT LANGUAGE C IMMUTABLE; 

CREATE OR REPLACE FUNCTION float8arr_cast_int2(int2) RETURNS float8[] AS 'gp_svec.so', 'float8arr_cast_int2' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION float8arr_cast_int4(int4) RETURNS float8[] AS 'gp_svec.so', 'float8arr_cast_int4' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION float8arr_cast_int8(bigint) RETURNS float8[] AS 'gp_svec.so', 'float8arr_cast_int8' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION float8arr_cast_float4(float4) RETURNS float8[] AS 'gp_svec.so', 'float8arr_cast_float4' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION float8arr_cast_float8(float8) RETURNS float8[] AS 'gp_svec.so', 'float8arr_cast_float8' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION float8arr_cast_numeric(numeric) RETURNS float8[] AS 'gp_svec.so', 'float8arr_cast_numeric' STRICT LANGUAGE C IMMUTABLE; 

CREATE OR REPLACE FUNCTION svec_cast_float8arr(float8[]) RETURNS svec AS 'gp_svec.so', 'svec_cast_float8arr' STRICT LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_return_array(svec) RETURNS float8[] AS 'gp_svec.so', 'svec_return_array' LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_concat(svec,svec) RETURNS svec AS 'gp_svec.so', 'svec_concat' LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION svec_concat_replicate(int4,svec) RETURNS svec AS 'gp_svec.so', 'svec_concat_replicate' LANGUAGE C IMMUTABLE; 
CREATE OR REPLACE FUNCTION dimension(svec) RETURNS integer AS 'gp_svec.so', 'svec_dimension' LANGUAGE C IMMUTABLE; 


CREATE OPERATOR || (
	LEFTARG = svec,
	RIGHTARG = svec,
	PROCEDURE = svec_concat
);

CREATE OPERATOR - (
	LEFTARG = svec,
	RIGHTARG = svec,
	PROCEDURE = svec_minus
);
CREATE OPERATOR + (
	LEFTARG = svec,
	RIGHTARG = svec,
	PROCEDURE = svec_plus
);
CREATE OPERATOR / (
	LEFTARG = svec,
	RIGHTARG = svec,
	PROCEDURE = svec_div
);
CREATE OPERATOR %*% (
	LEFTARG = svec,
	RIGHTARG = svec,
	PROCEDURE = dot
);
CREATE OPERATOR * (
	LEFTARG = svec,
	RIGHTARG = svec,
	PROCEDURE = svec_mult
);
CREATE OPERATOR ^ (
	LEFTARG = svec,
	RIGHTARG = svec,
	PROCEDURE = svec_pow
);

-- float8[] operators
DROP OPERATOR IF EXISTS = ( float8[], float8[]);
DROP OPERATOR IF EXISTS %*% ( float8[], svec);
DROP OPERATOR IF EXISTS %*% ( svec, float8[]);
DROP OPERATOR IF EXISTS %*% ( float8[], float8[]);
DROP OPERATOR IF EXISTS - ( float8[], float8[]);
DROP OPERATOR IF EXISTS + ( float8[], float8[]);
DROP OPERATOR IF EXISTS * ( float8[], float8[]);
DROP OPERATOR IF EXISTS / ( float8[], float8[]);
DROP OPERATOR IF EXISTS - ( float8[], svec);
DROP OPERATOR IF EXISTS + ( float8[], svec);
DROP OPERATOR IF EXISTS * ( float8[], svec);
DROP OPERATOR IF EXISTS / ( float8[], svec);
DROP OPERATOR IF EXISTS - ( svec, float8[]);
DROP OPERATOR IF EXISTS + ( svec, float8[]);
DROP OPERATOR IF EXISTS * ( svec, float8[]);
DROP OPERATOR IF EXISTS / ( svec, float8[]);

CREATE OPERATOR = (
	leftarg = float8[], rightarg = float8[], procedure = float8arr_eq,
	commutator = = ,
--	negator = <> ,
	restrict = eqsel, join = eqjoinsel
);
CREATE OPERATOR %*% (
	LEFTARG = float8[],
	RIGHTARG = float8[],
	PROCEDURE = dot
);
CREATE OPERATOR %*% (
	LEFTARG = float8[],
	RIGHTARG = svec,
	PROCEDURE = dot
);
CREATE OPERATOR %*% (
	LEFTARG = svec,
	RIGHTARG = float8[],
	PROCEDURE = dot
);
CREATE OPERATOR - (
	LEFTARG = float8[],
	RIGHTARG = float8[],
	PROCEDURE = float8arr_minus_float8arr
);
CREATE OPERATOR + (
	LEFTARG = float8[],
	RIGHTARG = float8[],
	PROCEDURE = float8arr_plus_float8arr
);
CREATE OPERATOR * (
	LEFTARG = float8[],
	RIGHTARG = float8[],
	PROCEDURE = float8arr_mult_float8arr
);
CREATE OPERATOR / (
	LEFTARG = float8[],
	RIGHTARG = float8[],
	PROCEDURE = float8arr_div_float8arr
);

CREATE OPERATOR - (
	LEFTARG = float8[],
	RIGHTARG = svec,
	PROCEDURE = float8arr_minus_svec
);
CREATE OPERATOR + (
	LEFTARG = float8[],
	RIGHTARG = svec,
	PROCEDURE = float8arr_plus_svec
);
CREATE OPERATOR * (
	LEFTARG = float8[],
	RIGHTARG = svec,
	PROCEDURE = float8arr_mult_svec
);
CREATE OPERATOR / (
	LEFTARG = float8[],
	RIGHTARG = svec,
	PROCEDURE = float8arr_div_svec
);

CREATE OPERATOR - (
	LEFTARG = svec,
	RIGHTARG = float8[],
	PROCEDURE = svec_minus_float8arr
);
CREATE OPERATOR + (
	LEFTARG = svec,
	RIGHTARG = float8[],
	PROCEDURE = svec_plus_float8arr
);
CREATE OPERATOR * (
	LEFTARG = svec,
	RIGHTARG = float8[],
	PROCEDURE = svec_mult_float8arr
);
CREATE OPERATOR / (
	LEFTARG = svec,
	RIGHTARG = float8[],
	PROCEDURE = svec_div_float8arr
);

CREATE CAST (int2 AS svec) WITH FUNCTION svec_cast_int2(int2) AS IMPLICIT;
CREATE CAST (integer AS svec) WITH FUNCTION svec_cast_int4(integer) AS IMPLICIT;
CREATE CAST (bigint AS svec) WITH FUNCTION svec_cast_int8(bigint) AS IMPLICIT;
CREATE CAST (float4 AS svec) WITH FUNCTION svec_cast_float4(float4) AS IMPLICIT;
CREATE CAST (float8 AS svec) WITH FUNCTION svec_cast_float8(float8) AS IMPLICIT;
CREATE CAST (numeric AS svec) WITH FUNCTION svec_cast_numeric(numeric) AS IMPLICIT;

DROP CAST IF EXISTS (int2 AS float8[]) ;
DROP CAST IF EXISTS (integer AS float8[]) ;
DROP CAST IF EXISTS (bigint AS float8[]) ;
DROP CAST IF EXISTS (float4 AS float8[]) ;
DROP CAST IF EXISTS (float8 AS float8[]) ;
DROP CAST IF EXISTS (numeric AS float8[]) ;

-- CREATE CAST (int2 AS float8[]) WITH FUNCTION float8arr_cast_int2(int2) AS IMPLICIT;
-- CREATE CAST (integer AS float8[]) WITH FUNCTION float8arr_cast_int4(integer) AS IMPLICIT;
-- CREATE CAST (bigint AS float8[]) WITH FUNCTION float8arr_cast_int8(bigint) AS IMPLICIT;
-- CREATE CAST (float4 AS float8[]) WITH FUNCTION float8arr_cast_float4(float4) AS IMPLICIT;
-- CREATE CAST (float8 AS float8[]) WITH FUNCTION float8arr_cast_float8(float8) AS IMPLICIT;
-- CREATE CAST (numeric AS float8[]) WITH FUNCTION float8arr_cast_numeric(numeric) AS IMPLICIT;

CREATE CAST (svec AS float8[]) WITH FUNCTION svec_return_array(svec) AS IMPLICIT;
CREATE CAST (float8[] AS svec) WITH FUNCTION svec_cast_float8arr(float8[]) AS IMPLICIT;

CREATE OPERATOR = (
	leftarg = svec, rightarg = svec, procedure = svec_eq,
	commutator = = ,
--	negator = <> ,
	restrict = eqsel, join = eqjoinsel
);

DROP AGGREGATE IF EXISTS sum(svec);
CREATE AGGREGATE sum (svec) (
	SFUNC = svec_plus,
	PREFUNC = svec_plus,
	INITCOND = '{1}:{0.}', -- Zero
	STYPE = svec
);

-- Aggregate that provides a tally of nonzero entries in a list of vectors
DROP AGGREGATE IF EXISTS count_vec(svec); -- Legacy name for vec_count_nonzero
CREATE AGGREGATE count_vec (svec) ( -- Legacy name for vec_count_nonzero
	SFUNC = svec_count, -- Legacy name for vec_count_nonzero
	PREFUNC = svec_plus, -- Legacy name for vec_count_nonzero
 	INITCOND = '{1}:{0.}', -- Zero -- Legacy name for vec_count_nonzero
	STYPE = svec -- Legacy name for vec_count_nonzero
);
DROP AGGREGATE IF EXISTS vec_count_nonzero(svec);
CREATE AGGREGATE vec_count_nonzero (svec) (
	SFUNC = svec_count,
	PREFUNC = svec_plus,
 	INITCOND = '{1}:{0.}', -- Zero
	STYPE = svec
);

DROP AGGREGATE IF EXISTS array_agg(float8);
CREATE AGGREGATE array_agg (float8) (
	SFUNC = vec_pivot,
	PREFUNC = svec_concat,
	STYPE = svec
);

DROP AGGREGATE IF EXISTS median_inmemory(float8);
CREATE AGGREGATE median_inmemory (float8) (
	SFUNC = vec_pivot,
	PREFUNC = svec_concat,
	FINALFUNC = vec_median,
	STYPE = svec
);

-- Comparisons based on L2 Norm
CREATE OR REPLACE FUNCTION svec_l2_lt(svec,svec) RETURNS bool AS 'gp_svec.so', 'svec_l2_lt' STRICT LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION svec_l2_le(svec,svec) RETURNS bool AS 'gp_svec.so', 'svec_l2_le' STRICT LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION svec_l2_eq(svec,svec) RETURNS bool AS 'gp_svec.so', 'svec_l2_eq' STRICT LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION svec_l2_ne(svec,svec) RETURNS bool AS 'gp_svec.so', 'svec_l2_ne' STRICT LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION svec_l2_gt(svec,svec) RETURNS bool AS 'gp_svec.so', 'svec_l2_gt' STRICT LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION svec_l2_ge(svec,svec) RETURNS bool AS 'gp_svec.so', 'svec_l2_ge' STRICT LANGUAGE C IMMUTABLE;
CREATE OR REPLACE FUNCTION svec_l2_cmp(svec,svec) RETURNS integer AS 'gp_svec.so', 'svec_l2_cmp' STRICT LANGUAGE C IMMUTABLE;

CREATE OPERATOR < (
	leftarg = svec, rightarg = svec, procedure = svec_l2_lt,
	commutator = > , negator = >= ,
	restrict = scalarltsel, join = scalarltjoinsel
);
CREATE OPERATOR <= (
	leftarg = svec, rightarg = svec, procedure = svec_l2_le,
	commutator = >= , negator = > ,
	restrict = scalarltsel, join = scalarltjoinsel
);
CREATE OPERATOR <> (
	leftarg = svec, rightarg = svec, procedure = svec_l2_eq,
	commutator = <> ,
	negator = =,
	restrict = eqsel, join = eqjoinsel
);
CREATE OPERATOR == (
	leftarg = svec, rightarg = svec, procedure = svec_l2_eq,
	commutator = = ,
	negator = <> ,
	restrict = eqsel, join = eqjoinsel
);
CREATE OPERATOR >= (
	leftarg = svec, rightarg = svec, procedure = svec_l2_ge,
	commutator = <= , negator = < ,
	restrict = scalargtsel, join = scalargtjoinsel
);
CREATE OPERATOR > (
	leftarg = svec, rightarg = svec, procedure = svec_l2_gt,
	commutator = < , negator = <= ,
	restrict = scalargtsel, join = scalargtjoinsel
);

CREATE OPERATOR *|| (
	leftarg = int4, rightarg = svec, procedure = svec_concat_replicate
);

CREATE OPERATOR CLASS svec_l2_ops
DEFAULT FOR TYPE svec USING btree AS
OPERATOR        1       < ,
OPERATOR        2       <= ,
OPERATOR        3       == ,
OPERATOR        4       >= ,
OPERATOR        5       > ,
FUNCTION        1       svec_l2_cmp(svec, svec);

SET search_path TO DEFAULT;
