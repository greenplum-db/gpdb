/* contrib/citext/citext--1.5--1.6.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION citext UPDATE TO '1.6'" to load this file. \quit

CREATE FUNCTION citext_hash_extended(citext, int8)
RETURNS int8
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

ALTER OPERATOR FAMILY citext_ops USING hash ADD
    FUNCTION    2   citext_hash_extended(citext, int8);

CREATE OPERATOR CLASS citext_ops
DEFAULT FOR TYPE CITEXT USING bitmap AS
    OPERATOR    1   <  (citext, citext),
    OPERATOR    2   <= (citext, citext),
    OPERATOR    3   =  (citext, citext),
    OPERATOR    4   >= (citext, citext),
    OPERATOR    5   >  (citext, citext),
    FUNCTION    1   citext_cmp(citext, citext);
