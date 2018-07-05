-- HyperLogLog 

CREATE SCHEMA gp_hyperloglog;

SET search_path = gp_hyperloglog, public, pg_catalog;

-- HyperLogLog counter (shell type)
CREATE TYPE hyperloglog_estimator;

/* input/output functions */
CREATE FUNCTION hyperloglog_in(value cstring) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_in'
     LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION hyperloglog_out(counter hyperloglog_estimator) RETURNS cstring
     AS '$libdir/hyperloglog_counter', 'hyperloglog_out'
     LANGUAGE C STRICT IMMUTABLE;

-- actual LogLog counter data type
CREATE TYPE hyperloglog_estimator (
    INPUT = hyperloglog_in,
    OUTPUT = hyperloglog_out,
    STORAGE = EXTENDED,
    ALIGNMENT = int4,
    INTERNALLENGTH = VARIABLE
);

-- allow cast from bytea to hyperloglog_estimator
CREATE CAST (bytea as hyperloglog_estimator) WITHOUT FUNCTION AS ASSIGNMENT;

-- allow cast from bytea to hyperloglog_estimator
CREATE CAST (hyperloglog_estimator as bytea) WITHOUT FUNCTION AS ASSIGNMENT;

CREATE OR REPLACE FUNCTION hyperloglog_to_text(hyperloglog_estimator) RETURNS text
AS $$ select pg_catalog.encode($1::bytea, 'base64'); $$ LANGUAGE SQL IMMUTABLE;

CREATE OR REPLACE FUNCTION hyperloglog_from_text(text) RETURNS hyperloglog_estimator
AS $$ select pg_catalog.decode($1, 'base64')::hyperloglog_estimator; $$ LANGUAGE SQL IMMUTABLE;

CREATE CAST (hyperloglog_estimator as text) WITH FUNCTION hyperloglog_to_text(hyperloglog_estimator);

CREATE CAST (text as hyperloglog_estimator) WITH FUNCTION hyperloglog_from_text(text);

/* compress/decompress inner data funcitons */
CREATE OR REPLACE FUNCTION hyperloglog_comp(counter hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_comp'
     LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION hyperloglog_comp(counter hyperloglog_estimator) IS 'Uses an internal quicklz/group varint encoding algorithm to compress each hyperloglog_estimator individually';

/* Utility functions */
-- creates a new HyperLogLog estimator with default error_rate 0.8125% and default 2^64 ndistinct
CREATE OR REPLACE FUNCTION hyperloglog_init() RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_init_default'
     LANGUAGE C IMMUTABLE;

-- merges the second estimator into a copy of the first one
CREATE OR REPLACE FUNCTION hyperloglog_merge(estimator1 hyperloglog_estimator, estimator2 hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_merge'
     LANGUAGE C IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_merge(estimator1 hyperloglog_estimator, estimator2 hyperloglog_estimator) IS 'Merge two seperate hyperloglog_estimators into one. This varies from the aggregate hyperloglog_merge since it merges columns together instead of rows.';

-- merges (inplace) the second estimator into the first one
CREATE OR REPLACE FUNCTION hyperloglog_merge_agg(estimator1 hyperloglog_estimator, estimator2 hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_merge'
     LANGUAGE C IMMUTABLE;

-- get current estimate of the distinct values (as a real number)
CREATE OR REPLACE FUNCTION hyperloglog_get_estimate(counter hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_get_estimate'
     LANGUAGE C STRICT IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_get_estimate(counter hyperloglog_estimator) IS 'Estimates the cardinality of the provided hyperloglog_estimator';

-- length of the estimator (about the same as hyperloglog_size with existing estimator)
CREATE OR REPLACE FUNCTION length(counter hyperloglog_estimator) RETURNS int
     AS '$libdir/hyperloglog_counter', 'hyperloglog_length'
     LANGUAGE C STRICT IMMUTABLE;
COMMENT ON FUNCTION length(counter hyperloglog_estimator) IS 'Returns the length of the provided hyperloglog_estimator which is equivalent to its size in bytes';
     
/* functions for aggregate functions */

CREATE OR REPLACE FUNCTION hyperloglog_add_item_agg_default(counter hyperloglog_estimator, item anyelement) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item_agg_default'
     LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION hyperloglog_get_estimate_bigint(hyperloglog_estimator) RETURNS bigint
     AS $$ select coalesce(round(hyperloglog_get_estimate($1))::bigint, 0) $$
     LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_get_estimate_bigint(hyperloglog_estimator) IS 'Gets the estimated cardinality of the hyperloglog_estimator and rounds to the nearest whole number (bigint)';

-- HyperLogLog based count distinct (item)
DROP AGGREGATE IF EXISTS  hyperloglog_distinct(anyelement);
CREATE AGGREGATE hyperloglog_distinct(anyelement)
(
    sfunc = hyperloglog_add_item_agg_default,
    stype = hyperloglog_estimator,
    prefunc = hyperloglog_merge_agg,
    finalfunc = hyperloglog_get_estimate
);
COMMENT ON AGGREGATE  hyperloglog_distinct(anyelement) IS 'Uses the hyperloglog_estimator to estimate the distinct count of the column with default capacity and accuracy values (0.8125% accuracy, 1.84*10^19 capacity)';

-- build the counter(s) from elements, but does not perform the final estimation
DROP AGGREGATE IF EXISTS hyperloglog_accum(anyelement);
CREATE AGGREGATE hyperloglog_accum(anyelement)
(
    sfunc = hyperloglog_add_item_agg_default,
    prefunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum(anyelement) IS 'Accumulates anyelement into a hyperloglog_estimator of default size (14 index bits, 6 bits/bucket)';

-- mirror real sum function
DROP AGGREGATE IF EXISTS sum(hyperloglog_estimator);
CREATE AGGREGATE sum(hyperloglog_estimator)
(
    sfunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    prefunc = hyperloglog_merge_agg,
    finalfunc = hyperloglog_get_estimate_bigint
);
COMMENT ON AGGREGATE sum(hyperloglog_estimator) IS 'First merges all the hyperloglog_estimators, then evaluates the resulting counter, and then rounds the result to the nearest whole number (bigint)';

-- merges all the counters into just a single one (e.g. after running hyperloglog_accum)
DROP AGGREGATE IF EXISTS hyperloglog_merge(hyperloglog_estimator);
CREATE AGGREGATE hyperloglog_merge(hyperloglog_estimator)
(
    sfunc = hyperloglog_merge_agg,
    prefunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_merge(hyperloglog_estimator) IS 'Merges/unions hyperloglog_estimators into a unified estimator';

GRANT USAGE ON SCHEMA gp_hyperloglog TO public;
