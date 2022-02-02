---------------------------------------------------------------------------
--
-- gp_array_agg.sql-
--    This file creates a gp_array_agg(anyelement) which is like array_agg
--    But gp_array_agg() supports parallel aggregation
--
--
-- Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
-- Portions Copyright (c) 1994, Regents of the University of California
--
--
---------------------------------------------------------------------------

-- Assume the user defined functions are in $libdir/gp_array_agg
-- (we do not want to assume this is in the dynamic loader search path).
-- Look at gp_array_agg.c for the source.  Note we mark them IMMUTABLE,
-- since they always return the same outputs given the same inputs.

CREATE FUNCTION array_agg_combine(internal, internal)
    RETURNS internal
AS '$libdir/gp_array_agg'
   LANGUAGE C IMMUTABLE;

CREATE FUNCTION array_agg_serialize(internal)
    RETURNS bytea
AS '$libdir/gp_array_agg'
   LANGUAGE C IMMUTABLE;

CREATE FUNCTION array_agg_deserialize(bytea, internal)
    RETURNS internal
AS '$libdir/gp_array_agg'
   LANGUAGE C IMMUTABLE;

-- Creating aggregate functions
CREATE AGGREGATE gp_array_agg(anyelement)
(
     SFUNC = array_agg_transfn,
     STYPE = internal,
     COMBINEFUNC = array_agg_combine,
     SERIALFUNC = array_agg_serialize,
     DESERIALFUNC = array_agg_deserialize,
     FINALFUNC = array_agg_finalfn,
     FINALFUNC_EXTRA
);
