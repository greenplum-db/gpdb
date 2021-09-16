---------------------------------------------------------------------------
--
-- gp_legacy_string_agg--1.0.0.sql-
--    This file creates a string_agg(text) which is deprecated in GPDB6 server.
--    Instead, users should convert their SQL to use string_agg(text, text).
--
--    E.g. string_agg('a') should converted to string_agg('a', '')
--
--
-- Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
-- Portions Copyright (c) 1994, Regents of the University of California
--
-- gpcontrib/gp_legacy_string_agg/gp_legacy_string_agg--1.0.0.sql
--
---------------------------------------------------------------------------

-- Assume the user defined functions are in $libdir/gp_legacy_string_agg
-- (we do not want to assume this is in the dynamic loader search path).
-- Look at gpcontrib/gp_legacy_string_agg/gp_legacy_string_agg.c for the
-- source.  Note we mark them IMMUTABLE, since they always return the same
-- outputs given the same inputs.


-- first, define a function gp_legacy_string_agg_transfn (also in gp_legacy_string_agg.c)
CREATE FUNCTION gp_legacy_string_agg_transfn(internal, text)
   RETURNS internal
   AS '$libdir/gp_legacy_string_agg'
   LANGUAGE C IMMUTABLE;

CREATE FUNCTION gp_legacy_string_agg_finalfn(internal)
   RETURNS text
   AS '$libdir/gp_legacy_string_agg'
   LANGUAGE C IMMUTABLE;


-- Creating aggregate functions
CREATE AGGREGATE string_agg(text)
(
     sfunc= gp_legacy_string_agg_transfn,
     stype= internal,
     finalfunc= gp_legacy_string_agg_finalfn
);
