-- HyperLogLog 

SET search_path = public, pg_catalog;

-- HyperLogLog counter (shell type)
CREATE TYPE hyperloglog_estimator;

/* input/output functions */
CREATE FUNCTION hyperloglog_in(value cstring) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_in'
     LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION hyperloglog_out(counter hyperloglog_estimator) RETURNS cstring
     AS '$libdir/hyperloglog_counter', 'hyperloglog_out'
     LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION hyperloglog_recv(internal) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_recv'
     LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION hyperloglog_send(hyperloglog_estimator) RETURNS bytea
     AS '$libdir/hyperloglog_counter', 'hyperloglog_send'
     LANGUAGE C STRICT IMMUTABLE;

-- actual LogLog counter data type
CREATE TYPE hyperloglog_estimator (
    INPUT = hyperloglog_in,
    OUTPUT = hyperloglog_out,
    STORAGE = EXTENDED,
    ALIGNMENT = int4,
    INTERNALLENGTH = VARIABLE,
    SEND = hyperloglog_send,
    RECEIVE = hyperloglog_recv
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

CREATE OR REPLACE FUNCTION hyperloglog_decomp(counter hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_decomp'
     LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION hyperloglog_decomp(counter hyperloglog_estimator) IS 'Decompress and return a bitpacked version of the hyperloglog_estimator';

CREATE OR REPLACE FUNCTION hyperloglog_unpack(counter hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_unpack'
     LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION hyperloglog_unpack(counter hyperloglog_estimator) IS 'Return an unpacked version of the hyperloglog_estimator';

/* Utility functions */
-- upgrades old counters into the new version
CREATE OR REPLACE FUNCTION  hyperloglog_update(counter hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_update'
     LANGUAGE C IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_update(counter hyperloglog_estimator) IS 'Updates older versions of hyperloglog_estimators to the latest version the plugin supports';

-- Gives info about the selected counter or about the current implementations counters
CREATE OR REPLACE FUNCTION  hyperloglog_info(counter hyperloglog_estimator) RETURNS TEXT
     AS '$libdir/hyperloglog_counter', 'hyperloglog_info'
     LANGUAGE C IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_info(counter hyperloglog_estimator) IS 'Provides details on the specific hyperloglog_estimator entered';

CREATE OR REPLACE FUNCTION  hyperloglog_info() RETURNS TEXT
     AS '$libdir/hyperloglog_counter', 'hyperloglog_info_noargs'
     LANGUAGE C IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_info() IS 'Provides general plugin information and settings';

-- get estimator size for the default error_rate 0.8125% and default 2^64 ndistinct
CREATE OR REPLACE FUNCTION hyperloglog_size() RETURNS int
     AS '$libdir/hyperloglog_counter', 'hyperloglog_size_default'
     LANGUAGE C IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_size() IS 'Gives the size of a hyperloglog_estimator using default accuracy and capacity values';
     
-- get estimator size for the requested error_rate and default 2^64 ndistinct
CREATE OR REPLACE FUNCTION hyperloglog_size(error_rate real) RETURNS int
     AS '$libdir/hyperloglog_counter', 'hyperloglog_size_error'
     LANGUAGE C IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_size(error_rate real) IS 'Gives the size of a hyperloglog_estimator with default capacity and a specified accuracy';

-- get estimator size for the requested error_rate and desired ndistinct
CREATE OR REPLACE FUNCTION hyperloglog_size(error_rate real, ndistinct double precision) RETURNS int
     AS '$libdir/hyperloglog_counter', 'hyperloglog_size'
     LANGUAGE C IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_size(error_rate real, ndistinct double precision) IS 'Gives the size of a hyperloglog_estimator with a specified capactiy and a specified accuracy';
     
-- creates a new HyperLogLog estimator with default error_rate 0.8125% and default 2^64 ndistinct
CREATE OR REPLACE FUNCTION hyperloglog_init() RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_init_default'
     LANGUAGE C IMMUTABLE;

-- creates a new HyperLogLog estimator with desired error_rate and default 2^64 ndistinct
CREATE OR REPLACE FUNCTION hyperloglog_init(error_rate real) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_init_error'
     LANGUAGE C IMMUTABLE;

-- creates a new HyperLogLog estimator with desired error_rate and a desired ndistinct
CREATE OR REPLACE FUNCTION hyperloglog_init(error_rate real,ndistinct double precision) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_init'
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

CREATE OR REPLACE FUNCTION hyperloglog_merge_unsafe(estimator1 hyperloglog_estimator, estimator2 hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_merge_unsafe'
     LANGUAGE C IMMUTABLE;


-- add an item to the estimator
CREATE OR REPLACE FUNCTION hyperloglog_add_item(counter hyperloglog_estimator, item anyelement) RETURNS void
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item'
     LANGUAGE C IMMUTABLE;

-- get current estimate of the distinct values (as a real number)
CREATE OR REPLACE FUNCTION hyperloglog_get_estimate(counter hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_get_estimate'
     LANGUAGE C STRICT IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_get_estimate(counter hyperloglog_estimator) IS 'Estimates the cardinality of the provided hyperloglog_estimator';

-- reset the estimator (start counting from the beginning)
CREATE OR REPLACE FUNCTION hyperloglog_reset(counter hyperloglog_estimator) RETURNS void
     AS '$libdir/hyperloglog_counter', 'hyperloglog_reset'
     LANGUAGE C STRICT IMMUTABLE;

-- length of the estimator (about the same as hyperloglog_size with existing estimator)
CREATE OR REPLACE FUNCTION length(counter hyperloglog_estimator) RETURNS int
     AS '$libdir/hyperloglog_counter', 'hyperloglog_length'
     LANGUAGE C STRICT IMMUTABLE;
COMMENT ON FUNCTION length(counter hyperloglog_estimator) IS 'Returns the length of the provided hyperloglog_estimator which is equivalent to its size in bytes';
     
/* functions for set operations */
CREATE OR REPLACE FUNCTION hyperloglog_equal(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS bool
     AS '$libdir/hyperloglog_counter', 'hyperloglog_equal'
     LANGUAGE C STRICT IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_equal(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) IS 'Does a deep equality check that checks every bucket in the hyperloglog_estimators for equality';

CREATE OR REPLACE FUNCTION hyperloglog_not_equal(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS bool
     AS '$libdir/hyperloglog_counter', 'hyperloglog_not_equal'
     LANGUAGE C STRICT IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_not_equal(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) IS 'Does a deep equality check that checks every bucket in the hyperloglog_estimators for inequality';

CREATE OR REPLACE FUNCTION hyperloglog_intersection(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_intersection'
     LANGUAGE C STRICT IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_intersection(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) IS 'Uses the inclusion-exclusion principle to estimate the intersection of two hyperloglog_estimators. Its worth noting that the error will be relative to the largest hyperloglog_estimator provided.';

CREATE OR REPLACE FUNCTION hyperloglog_union(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_union'
     LANGUAGE C IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_union(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) IS 'Performs a merge and then an evaluation of the two provided hyperloglog_estimators. Accuracy guarantees are maintained.';

CREATE OR REPLACE FUNCTION hyperloglog_compliment(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_compliment'
     LANGUAGE C IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_compliment(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) IS 'Uses the inclusion-exclusion principle to estimate the compliment of two hyperloglog_estimators. Its worth noting that the error will be relative to the largest hyperloglog_estimator provided.';

CREATE OR REPLACE FUNCTION hyperloglog_symmetric_diff(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_symmetric_diff'
     LANGUAGE C IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_symmetric_diff(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) IS 'Uses the inclusion-exclusion principle to estimate the symmetric difference of two hyperloglog_estimators. Its worth noting that the error will be relative to the largest hyperloglog_estimator provided.';

/* functions for aggregate functions */

CREATE OR REPLACE FUNCTION hyperloglog_add_item_agg(counter hyperloglog_estimator, item anyelement, error_rate real, ndistinct double precision) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item_agg'
     LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION hyperloglog_add_item_agg(counter hyperloglog_estimator, item anyelement, error_rate real, ndistinct double precision, format text) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item_agg_pack'
     LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION hyperloglog_add_item_agg_error(counter hyperloglog_estimator, item anyelement, error_rate real) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item_agg_error'
     LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION hyperloglog_add_item_agg_error(counter hyperloglog_estimator, item anyelement, error_rate real,format text) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item_agg_error_pack'
     LANGUAGE C IMMUTABLE;

     
CREATE OR REPLACE FUNCTION hyperloglog_add_item_agg_default(counter hyperloglog_estimator, item anyelement) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item_agg_default'
     LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION hyperloglog_add_item_agg_default(counter hyperloglog_estimator, item anyelement, format text) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item_agg_default_pack'
     LANGUAGE C IMMUTABLE;
     
CREATE OR REPLACE FUNCTION hyperloglog_get_estimate_bigint(hyperloglog_estimator) RETURNS bigint
     AS $$ select coalesce(round(hyperloglog_get_estimate($1))::bigint, 0) $$
     LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_get_estimate_bigint(hyperloglog_estimator) IS 'Gets the estimated cardinality of the hyperloglog_estimator and rounds to the nearest whole number (bigint)';

/* functions for operators */
CREATE OR REPLACE FUNCTION convert_to_scalar(hyperloglog_estimator) RETURNS bigint
    AS $$ select coalesce(hyperloglog_get_estimate($1)::bigint,0) $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION convert_to_scalar(hyperloglog_estimator) IS 'Gets the estimated cardinality of the hyperloglog_estimator and rounds to the nearest whole number (bigint)';

CREATE OR REPLACE FUNCTION hyperloglog_greater_than(hyperloglog_estimator,hyperloglog_estimator) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) > hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_greater_than(hyperloglog_estimator,hyperloglog_estimator) IS 'Compares the estimated cardinalities of the provided hyperloglog_estimators and returns true if the first (left argument) is greater than the second (right argument)';

CREATE OR REPLACE FUNCTION hyperloglog_greater_than(hyperloglog_estimator,NUMERIC) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) > $2 $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_greater_than(hyperloglog_estimator,NUMERIC) IS 'Compares the estimated cardinality of the provided hyperloglog_estimator and the numeric value and returns true if the first (left argument) is greater than the second (right argument)';

CREATE OR REPLACE FUNCTION hyperloglog_greater_than(NUMERIC,hyperloglog_estimator) RETURNS bool
    AS $$ select $1 > hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_greater_than(NUMERIC,hyperloglog_estimator) IS 'Compares the estimated cardinality of the provided hyperloglog_estimator and the numeric value and returns true if the first (left argument) is greater than the second (right argument)';


CREATE OR REPLACE FUNCTION hyperloglog_less_than(hyperloglog_estimator,hyperloglog_estimator) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) < hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_less_than(hyperloglog_estimator,hyperloglog_estimator) IS 'Compares the estimated cardinalities of the provided hyperloglog_estimators and returns true if the first (left argument) is less than the second (right argument)';

CREATE OR REPLACE FUNCTION hyperloglog_less_than(hyperloglog_estimator,NUMERIC) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) < $2 $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_less_than(hyperloglog_estimator,NUMERIC) IS 'Compares the estimated cardinality of the provided hyperloglog_estimator and the numeric value and returns true if the first (left argument) is less than the second (right argument)';

CREATE OR REPLACE FUNCTION hyperloglog_less_than(NUMERIC,hyperloglog_estimator) RETURNS bool
    AS $$ select $1 < hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_less_than(NUMERIC,hyperloglog_estimator) IS 'Compares the estimated cardinality of the provided hyperloglog_estimator and the numeric value and returns true if the first (left argument) is less than the second (right argument)';

CREATE OR REPLACE FUNCTION hyperloglog_greater_than_equal(hyperloglog_estimator,hyperloglog_estimator) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) >= hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_greater_than_equal(hyperloglog_estimator,hyperloglog_estimator) IS 'Compares the estimated cardinalities of the provided hyperloglog_estimators and returns true if the first (left argument) is greater than or equal to the second (right argument)';

CREATE OR REPLACE FUNCTION hyperloglog_greater_than_equal(hyperloglog_estimator,NUMERIC) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) >= $2 $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_greater_than_equal(hyperloglog_estimator,NUMERIC) IS 'Compares the estimated cardinality of the provided hyperloglog_estimator and the numeric value and returns true if the first (left argument) is greater than or equal to the second (right argument)';

CREATE OR REPLACE FUNCTION hyperloglog_greater_than_equal(NUMERIC,hyperloglog_estimator) RETURNS bool
    AS $$ select $1 >= hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_greater_than_equal(NUMERIC,hyperloglog_estimator) IS 'Compares the estimated cardinality of the provided hyperloglog_estimator and the numeric value and returns true if the first (left argument) is greater than or equal to the second (right argument)';

CREATE OR REPLACE FUNCTION hyperloglog_less_than_equal(hyperloglog_estimator,hyperloglog_estimator) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) <= hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_less_than_equal(hyperloglog_estimator,hyperloglog_estimator) IS 'Compares the estimated cardinalities of the provided hyperloglog_estimators and returns true if the first (left argument) is less than or equal to the second (right argument)';

CREATE OR REPLACE FUNCTION hyperloglog_less_than_equal(hyperloglog_estimator,NUMERIC) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) <= $2 $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_less_than_equal(hyperloglog_estimator,NUMERIC) IS 'Compares the estimated cardinality of the provided hyperloglog_estimator and the numeric value and returns true if the first (left argument) is less than or equal to the second (right argument)';

CREATE OR REPLACE FUNCTION hyperloglog_less_than_equal(NUMERIC,hyperloglog_estimator) RETURNS bool
    AS $$ select $1 <= hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;
COMMENT ON FUNCTION hyperloglog_less_than_equal(NUMERIC,hyperloglog_estimator) IS 'Compares the estimated cardinality of the provided hyperloglog_estimator and the numeric value and returns true if the first (left argument) is less than or equal to the second (right argument)';

-- HyperLogLog based count distinct (item, error rate, ndistinct)
DROP AGGREGATE IF EXISTS hyperloglog_distinct(anyelement, real , double precision);
CREATE AGGREGATE hyperloglog_distinct(anyelement, real , double precision)
(
    sfunc = hyperloglog_add_item_agg,
    stype = hyperloglog_estimator,
    prefunc = hyperloglog_merge_agg,
    finalfunc = hyperloglog_get_estimate
);
COMMENT ON AGGREGATE  hyperloglog_distinct(anyelement, real , double precision) IS 'Uses the hyperloglog_estimator to estimate the distinct count of the column with a specified accuracy and capacity value';

-- HyperLogLog based count distinct (item, error rate)
DROP AGGREGATE IF EXISTS hyperloglog_distinct(anyelement, real);
CREATE AGGREGATE hyperloglog_distinct(anyelement, real)
(
    sfunc = hyperloglog_add_item_agg_error,
    stype = hyperloglog_estimator,
    prefunc = hyperloglog_merge_agg,
    finalfunc = hyperloglog_get_estimate
);
COMMENT ON AGGREGATE hyperloglog_distinct(anyelement, real) IS 'Uses the hyperloglog_estimator to estimate the distinct count of the column with default capacity and a specified accuracy (specified accuracy, 1.84*10^19 capacity)';

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
DROP AGGREGATE IF EXISTS hyperloglog_accum(anyelement, real , double precision);
CREATE AGGREGATE hyperloglog_accum(anyelement, real , double precision)
(
    sfunc = hyperloglog_add_item_agg,
    prefunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum(anyelement,real,double precision) IS 'Accumulates anyelement into a hyperloglog_estimator of a specified capacity but and specified accuracy (variable index bits, variable bits/bucket)';

DROP AGGREGATE IF EXISTS hyperloglog_accum(anyelement, real , double precision, text);
CREATE AGGREGATE hyperloglog_accum(anyelement, real , double precision, text)
(
    sfunc = hyperloglog_add_item_agg,
    prefunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum(anyelement,real,double precision,text) IS 'Accumulates anyelement into a hyperloglog_estimator of a specified capacity but and specified accuracy (variable index bits, variable bits/bucket, format U|P)';

DROP AGGREGATE IF EXISTS hyperloglog_accum_unsafe(anyelement, real , double precision);
CREATE AGGREGATE hyperloglog_accum_unsafe(anyelement, real , double precision)
(
    sfunc = hyperloglog_add_item_agg,
    prefunc = hyperloglog_merge_unsafe,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum_unsafe(anyelement,real,double precision) IS 'Accumulates anyelement into a hyperloglog_estimator of a specified capacity but and specified accuracy (variable index bits, variable bits/bucket)';

DROP AGGREGATE IF EXISTS hyperloglog_accum_unsafe(anyelement, real , double precision, text);
CREATE AGGREGATE hyperloglog_accum_unsafe(anyelement, real , double precision, text)
(
    sfunc = hyperloglog_add_item_agg,
    prefunc = hyperloglog_merge_unsafe,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum_unsafe(anyelement,real,double precision,text) IS 'Accumulates anyelement into a hyperloglog_estimator of a specified capacity but and specified accuracy (variable index bits, variable bits/bucket, format U|P)';



DROP AGGREGATE IF EXISTS hyperloglog_accum(anyelement, real);
CREATE AGGREGATE hyperloglog_accum(anyelement, real)
(
    sfunc = hyperloglog_add_item_agg_error,
    prefunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum(anyelement,real) IS 'Accumulates anyelement into a hyperloglog_estimator of default capacity but and specified accuracy (variable index bits, 6 bits/bucket)';

DROP AGGREGATE IF EXISTS hyperloglog_accum(anyelement, real, text);
CREATE AGGREGATE hyperloglog_accum(anyelement, real, text)
(
    sfunc = hyperloglog_add_item_agg_error,
    prefunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum(anyelement,real,text) IS 'Accumulates anyelement into a hyperloglog_estimator of default capacity but and specified accuracy (variable index bits, 6 bits/bucket, format U|P)';

DROP AGGREGATE IF EXISTS hyperloglog_accum_unsafe(anyelement, real);
CREATE AGGREGATE hyperloglog_accum_unsafe(anyelement, real)
(
    sfunc = hyperloglog_add_item_agg_error,
    prefunc = hyperloglog_merge_unsafe,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum_unsafe(anyelement,real) IS 'Accumulates anyelement into a hyperloglog_estimator of default capacity but and specified accuracy (variable index bits, 6 bits/bucket)';

DROP AGGREGATE IF EXISTS hyperloglog_accum_unsafe(anyelement, real, text);
CREATE AGGREGATE hyperloglog_accum_unsafe(anyelement, real, text)
(
    sfunc = hyperloglog_add_item_agg_error,
    prefunc = hyperloglog_merge_unsafe,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum_unsafe(anyelement,real,text) IS 'Accumulates anyelement into a hyperloglog_estimator of default capacity but and specified accuracy (variable index bits, 6 bits/bucket, format U|P)';


DROP AGGREGATE IF EXISTS hyperloglog_accum(anyelement);
CREATE AGGREGATE hyperloglog_accum(anyelement)
(
    sfunc = hyperloglog_add_item_agg_default,
    prefunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum(anyelement) IS 'Accumulates anyelement into a hyperloglog_estimator of default size (14 index bits, 6 bits/bucket)';

DROP AGGREGATE IF EXISTS hyperloglog_accum(anyelement, text);
CREATE AGGREGATE hyperloglog_accum(anyelement, text)
(
    sfunc = hyperloglog_add_item_agg_default,
    prefunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum(anyelement,text) IS 'Accumulates anyelement into a hyperloglog_estimator of default capacity but and specified accuracy (variable index bits, 6 bits/bucket, format U|P)';

DROP AGGREGATE IF EXISTS hyperloglog_accum_unsafe(anyelement);
CREATE AGGREGATE hyperloglog_accum_unsafe(anyelement)
(
    sfunc = hyperloglog_add_item_agg_default,
    prefunc = hyperloglog_merge_unsafe,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum_unsafe(anyelement) IS 'Accumulates anyelement into a hyperloglog_estimator of default size (14 index bits, 6 bits/bucket)';

DROP AGGREGATE IF EXISTS hyperloglog_accum_unsafe(anyelement, text);
CREATE AGGREGATE hyperloglog_accum_unsafe(anyelement, text)
(
    sfunc = hyperloglog_add_item_agg_default,
    prefunc = hyperloglog_merge_unsafe,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_accum_unsafe(anyelement,text) IS 'Accumulates anyelement into a hyperloglog_estimator of default capacity but and specified accuracy (variable index bits, 6 bits/bucket, format U|P)';


-- mirror real sum function
DROP AGGREGATE IF EXISTS sum_unsafe(hyperloglog_estimator);
CREATE AGGREGATE sum_unsafe(hyperloglog_estimator)
(
    sfunc = hyperloglog_merge_unsafe,
    stype = hyperloglog_estimator,
    prefunc = hyperloglog_merge_unsafe,
    finalfunc = hyperloglog_get_estimate_bigint
);
COMMENT ON AGGREGATE sum_unsafe(hyperloglog_estimator) IS 'An optimized version of the sum(hyperloglog_estimator) aggregate that does not support use repeated use in the same SELECT statement or in UNION/UNION ALL queries';

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

-- merges all the counters into just a single one (e.g. after running hyperloglog_accum)
DROP AGGREGATE IF EXISTS hyperloglog_merge_unsafe(hyperloglog_estimator);
CREATE AGGREGATE hyperloglog_merge_unsafe(hyperloglog_estimator)
(
    sfunc = hyperloglog_merge_unsafe,
    prefunc = hyperloglog_merge_unsafe,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);
COMMENT ON AGGREGATE hyperloglog_merge_unsafe(hyperloglog_estimator) IS 'Merges/unions hyperloglog_estimators into a unified estimator';

-- evaluates the estimate (for an estimator)
CREATE OPERATOR # (
    PROCEDURE = hyperloglog_get_estimate,
    RIGHTARG = hyperloglog_estimator
);
COMMENT ON OPERATOR # (NONE,hyperloglog_estimator) IS 'Gives the estimated cardinality of the provided hyperloglog_estimator';

-- merges two estimators into a new one
CREATE OPERATOR || (
    PROCEDURE = hyperloglog_merge,
    LEFTARG  = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = ||
);
COMMENT ON OPERATOR || (hyperloglog_estimator,hyperloglog_estimator) IS 'Merges/unions two hyperloglog_estimators into one';

-- equality operator
CREATE OPERATOR = (
    PROCEDURE = hyperloglog_equal,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '=', NEGATOR = '<>',
    RESTRICT = eqsel, JOIN = eqjoinsel
);
COMMENT ON OPERATOR = (hyperloglog_estimator,hyperloglog_estimator) IS 'Returns true if the provided hyperloglog_estimators are equal';

-- not-equal operator
CREATE OPERATOR <> (
    PROCEDURE = hyperloglog_not_equal,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '<>' , NEGATOR = '=',
    RESTRICT = neqsel, JOIN = neqjoinsel
);
COMMENT ON OPERATOR <> (hyperloglog_estimator,hyperloglog_estimator) IS 'Returns true if the provided hyperloglog_estimators are not equal';

-- greater than operator
CREATE OPERATOR > (
    PROCEDURE = hyperloglog_greater_than,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '>' , NEGATOR = '<=',
    RESTRICT = scalargtsel, JOIN = scalargtjoinsel
);
COMMENT ON OPERATOR > (hyperloglog_estimator,hyperloglog_estimator) IS 'Returns true if the cardinality of the left hyperloglog_estimator is greater than the right hyperloglog_estimator';

CREATE OPERATOR > (
    PROCEDURE = hyperloglog_greater_than,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = NUMERIC,
    COMMUTATOR = '>' , NEGATOR = '<=',
    RESTRICT = scalargtsel, JOIN = scalargtjoinsel
);
COMMENT ON OPERATOR > (hyperloglog_estimator,NUMERIC) IS 'Returns true if the cardinality of the left hyperloglog_estimator is greater than the right numeric value';

CREATE OPERATOR > (
    PROCEDURE = hyperloglog_greater_than,
    LEFTARG = NUMERIC,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '>' , NEGATOR = '<=',
    RESTRICT = scalargtsel, JOIN = scalargtjoinsel
);
COMMENT ON OPERATOR > (NUMERIC,hyperloglog_estimator) IS 'Returns true if the cardinality of the left numeric is greater than the right hyperloglog_estimator';

-- less than operator
CREATE OPERATOR < (
    PROCEDURE = hyperloglog_less_than,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '<' , NEGATOR = '>=',
    RESTRICT = scalarltsel, JOIN = scalarltjoinsel
);
COMMENT ON OPERATOR < (hyperloglog_estimator,hyperloglog_estimator) IS 'Returns true if the cardinality of the left hyperloglog_estimator is less than the right hyperloglog_estimator';

CREATE OPERATOR < (
    PROCEDURE = hyperloglog_less_than,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = NUMERIC,
    COMMUTATOR = '<' , NEGATOR = '>=',
    RESTRICT = scalarltsel, JOIN = scalarltjoinsel
);
COMMENT ON OPERATOR < (hyperloglog_estimator,NUMERIC) IS 'Returns true if the cardinality of the left hyperloglog_estimator is less than the right numeric';

CREATE OPERATOR < (
    PROCEDURE = hyperloglog_less_than,
    LEFTARG = NUMERIC,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '<' , NEGATOR = '>=',
    RESTRICT = scalarltsel, JOIN = scalarltjoinsel
);
COMMENT ON OPERATOR < (NUMERIC,hyperloglog_estimator) IS 'Returns true if the cardinality of the left numeric is less than the right hyperloglog_estimator';

-- greater than or equal to operator
CREATE OPERATOR >= (
    PROCEDURE = hyperloglog_greater_than_equal,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '>=' , NEGATOR = '<',
    RESTRICT = scalargtsel, JOIN = scalargtjoinsel
);
COMMENT ON OPERATOR >= (hyperloglog_estimator,hyperloglog_estimator) IS 'Returns true if the cardinality of the left hyperloglog_estimator is greater than or equal to the right hyperloglog_estimator';

CREATE OPERATOR >= (
    PROCEDURE = hyperloglog_greater_than_equal,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = NUMERIC,
    COMMUTATOR = '>=' , NEGATOR = '<',
    RESTRICT = scalargtsel, JOIN = scalargtjoinsel
);
COMMENT ON OPERATOR >= (hyperloglog_estimator,NUMERIC) IS 'Returns true if the cardinality of the left hyperloglog_estimator is greater than or equal to the right numeric';

CREATE OPERATOR >= (
    PROCEDURE = hyperloglog_greater_than_equal,
    LEFTARG = NUMERIC,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '>=' , NEGATOR = '<',
    RESTRICT = scalargtsel, JOIN = scalargtjoinsel
);
COMMENT ON OPERATOR >= (NUMERIC,hyperloglog_estimator) IS 'Returns true if the cardinality of the left numeric is greater than or equal to the right hyperloglog_estimator';

-- less than or equal to operator
CREATE OPERATOR <= (
    PROCEDURE = hyperloglog_less_than_equal,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '<=' , NEGATOR = '>',
    RESTRICT = scalarltsel, JOIN = scalarltjoinsel
);
COMMENT ON OPERATOR <= (hyperloglog_estimator,hyperloglog_estimator) IS 'Returns true if the cardinality of the left hyperloglog_estimator is less than or equal to the right hyperloglog_estimator';

CREATE OPERATOR <= (
    PROCEDURE = hyperloglog_less_than_equal,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = NUMERIC,
    COMMUTATOR = '<=' , NEGATOR = '>',
    RESTRICT = scalarltsel, JOIN = scalarltjoinsel
);
COMMENT ON OPERATOR <= (hyperloglog_estimator,NUMERIC) IS 'Returns true if the cardinality of the left hyperloglog_estimator is less than or equal to the right numeric';

CREATE OPERATOR <= (
    PROCEDURE = hyperloglog_less_than_equal,
    LEFTARG = NUMERIC,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '<=' , NEGATOR = '>',
    RESTRICT = scalarltsel, JOIN = scalarltjoinsel
);
COMMENT ON OPERATOR <= (NUMERIC,hyperloglog_estimator) IS 'Returns true if the cardinality of the left numeric is less than or equal to the right hyperloglog_estimator';

