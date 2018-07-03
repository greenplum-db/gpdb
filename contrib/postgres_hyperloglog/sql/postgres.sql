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

/* compress/decompress inner data funcitons */
CREATE FUNCTION hyperloglog_comp(counter hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_comp'
     LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION hyperloglog_decomp(counter hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_decomp'
     LANGUAGE C IMMUTABLE STRICT;

/* Utility functions */
-- upgrades old counters into the new version
CREATE FUNCTION  hyperloglog_update(counter hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_update'
     LANGUAGE C IMMUTABLE;

-- Gives info about the selected counter or about the current implementations counters
CREATE FUNCTION  hyperloglog_info(counter hyperloglog_estimator) RETURNS TEXT
     AS '$libdir/hyperloglog_counter', 'hyperloglog_info'
     LANGUAGE C IMMUTABLE;

CREATE FUNCTION  hyperloglog_info() RETURNS TEXT
     AS '$libdir/hyperloglog_counter', 'hyperloglog_info_noargs'
     LANGUAGE C IMMUTABLE;

-- get estimator size for the default error_rate 0.8125% and default 2^64 ndistinct
CREATE FUNCTION hyperloglog_size() RETURNS int
     AS '$libdir/hyperloglog_counter', 'hyperloglog_size_default'
     LANGUAGE C IMMUTABLE;
     
-- get estimator size for the requested error_rate and default 2^64 ndistinct
CREATE FUNCTION hyperloglog_size(error_rate real) RETURNS int
     AS '$libdir/hyperloglog_counter', 'hyperloglog_size_error'
     LANGUAGE C IMMUTABLE;

-- get estimator size for the requested error_rate and desired ndistinct
CREATE FUNCTION hyperloglog_size(error_rate real, ndistinct double precision) RETURNS int
     AS '$libdir/hyperloglog_counter', 'hyperloglog_size'
     LANGUAGE C IMMUTABLE;
     
-- creates a new HyperLogLog estimator with default error_rate 0.8125% and default 2^64 ndistinct
CREATE FUNCTION hyperloglog_init() RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_init_default'
     LANGUAGE C IMMUTABLE;

-- creates a new HyperLogLog estimator with desired error_rate and default 2^64 ndistinct
CREATE FUNCTION hyperloglog_init(error_rate real) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_init_error'
     LANGUAGE C IMMUTABLE;

-- creates a new HyperLogLog estimator with desired error_rate and a desired ndistinct
CREATE FUNCTION hyperloglog_init(error_rate real,ndistinct double precision) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_init'
     LANGUAGE C IMMUTABLE;

-- merges the second estimator into a copy of the first one
CREATE FUNCTION hyperloglog_merge(estimator1 hyperloglog_estimator, estimator2 hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_merge'
     LANGUAGE C IMMUTABLE;

-- merges (inplace) the second estimator into the first one
CREATE FUNCTION hyperloglog_merge_agg(estimator1 hyperloglog_estimator, estimator2 hyperloglog_estimator) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_merge'
     LANGUAGE C IMMUTABLE;

-- add an item to the estimator
CREATE FUNCTION hyperloglog_add_item(counter hyperloglog_estimator, item anyelement) RETURNS void
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item'
     LANGUAGE C IMMUTABLE;

-- get current estimate of the distinct values (as a real number)
CREATE FUNCTION hyperloglog_get_estimate(counter hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_get_estimate'
     LANGUAGE C STRICT IMMUTABLE;

-- reset the estimator (start counting from the beginning)
CREATE FUNCTION hyperloglog_reset(counter hyperloglog_estimator) RETURNS void
     AS '$libdir/hyperloglog_counter', 'hyperloglog_reset'
     LANGUAGE C STRICT IMMUTABLE;

-- length of the estimator (about the same as hyperloglog_size with existing estimator)
CREATE FUNCTION length(counter hyperloglog_estimator) RETURNS int
     AS '$libdir/hyperloglog_counter', 'hyperloglog_length'
     LANGUAGE C STRICT IMMUTABLE;
     
/* functions for set operations */
CREATE FUNCTION hyperloglog_equal(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS bool
     AS '$libdir/hyperloglog_counter', 'hyperloglog_equal'
     LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION hyperloglog_not_equal(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS bool
     AS '$libdir/hyperloglog_counter', 'hyperloglog_not_equal'
     LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION hyperloglog_intersection(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_intersection'
     LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION hyperloglog_union(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_union'
     LANGUAGE C IMMUTABLE;

CREATE FUNCTION hyperloglog_compliment(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_compliment'
     LANGUAGE C IMMUTABLE;

CREATE FUNCTION hyperloglog_symmetric_diff(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator) RETURNS double precision
     AS '$libdir/hyperloglog_counter', 'hyperloglog_symmetric_diff'
     LANGUAGE C IMMUTABLE;

/* functions for aggregate functions */

CREATE FUNCTION hyperloglog_add_item_agg(counter hyperloglog_estimator, item anyelement, error_rate real, ndistinct double precision) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item_agg'
     LANGUAGE C IMMUTABLE;

CREATE FUNCTION hyperloglog_add_item_agg_error(counter hyperloglog_estimator, item anyelement, error_rate real) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item_agg_error'
     LANGUAGE C IMMUTABLE;
     
CREATE FUNCTION hyperloglog_add_item_agg_default(counter hyperloglog_estimator, item anyelement) RETURNS hyperloglog_estimator
     AS '$libdir/hyperloglog_counter', 'hyperloglog_add_item_agg_default'
     LANGUAGE C IMMUTABLE;
     
CREATE FUNCTION hyperloglog_get_estimate_bigint(hyperloglog_estimator) RETURNS bigint
     AS $$ select coalesce(round(hyperloglog_get_estimate($1))::bigint, 0) $$
     LANGUAGE SQL IMMUTABLE;

/* functions for operators */
CREATE FUNCTION convert_to_scalar(hyperloglog_estimator) RETURNS bigint
    AS $$ select coalesce(hyperloglog_get_estimate($1)::bigint,0) $$
    LANGUAGE SQL IMMUTABLE;

CREATE FUNCTION hyperloglog_greater_than(hyperloglog_estimator,hyperloglog_estimator) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) > hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;

CREATE FUNCTION hyperloglog_less_than(hyperloglog_estimator,hyperloglog_estimator) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) < hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;

CREATE FUNCTION hyperloglog_greater_than_equal(hyperloglog_estimator,hyperloglog_estimator) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) >= hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;

CREATE FUNCTION hyperloglog_less_than_equal(hyperloglog_estimator,hyperloglog_estimator) RETURNS bool
    AS $$ select hyperloglog_get_estimate($1) <= hyperloglog_get_estimate($2) $$
    LANGUAGE SQL IMMUTABLE;

-- HyperLogLog based count distinct (item, error rate, ndistinct)
CREATE AGGREGATE hyperloglog_distinct(anyelement, real , double precision)
(
    sfunc = hyperloglog_add_item_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_get_estimate
);

-- HyperLogLog based count distinct (item, error rate)
CREATE AGGREGATE hyperloglog_distinct(anyelement, real)
(
    sfunc = hyperloglog_add_item_agg_error,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_get_estimate
);

-- HyperLogLog based count distinct (item)
CREATE AGGREGATE hyperloglog_distinct(anyelement)
(
    sfunc = hyperloglog_add_item_agg_default,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_get_estimate
);

-- build the counter(s) from elements, but does not perform the final estimation
CREATE AGGREGATE hyperloglog_accum(anyelement, real , double precision)
(
    sfunc = hyperloglog_add_item_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);

CREATE AGGREGATE hyperloglog_accum(anyelement, real)
(
    sfunc = hyperloglog_add_item_agg_error,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);

CREATE AGGREGATE hyperloglog_accum(anyelement)
(
    sfunc = hyperloglog_add_item_agg_default,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);

-- mirror real sum function
CREATE AGGREGATE sum(hyperloglog_estimator)
(
    sfunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_get_estimate_bigint
);


-- merges all the counters into just a single one (e.g. after running hyperloglog_accum)
CREATE AGGREGATE hyperloglog_merge(hyperloglog_estimator)
(
    sfunc = hyperloglog_merge_agg,
    stype = hyperloglog_estimator,
    finalfunc = hyperloglog_comp
);

-- evaluates the estimate (for an estimator)
CREATE OPERATOR # (
    PROCEDURE = hyperloglog_get_estimate,
    RIGHTARG = hyperloglog_estimator
);

-- merges two estimators into a new one
CREATE OPERATOR || (
    PROCEDURE = hyperloglog_merge,
    LEFTARG  = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = ||
);

-- equality operator
CREATE OPERATOR = (
    PROCEDURE = hyperloglog_equal,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '=', NEGATOR = '<>',
    RESTRICT = eqsel, JOIN = eqjoinsel
);

-- not-equal operator
CREATE OPERATOR <> (
    PROCEDURE = hyperloglog_not_equal,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '<>' , NEGATOR = '=',
    RESTRICT = neqsel, JOIN = neqjoinsel
);

-- greater than operator
CREATE OPERATOR > (
    PROCEDURE = hyperloglog_greater_than,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '>' , NEGATOR = '<=',
    RESTRICT = scalargtsel, JOIN = scalargtjoinsel
);

-- less than operator
CREATE OPERATOR < (
    PROCEDURE = hyperloglog_less_than,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '<' , NEGATOR = '>=',
    RESTRICT = scalarltsel, JOIN = scalarltjoinsel
);

-- greater than or equal to operator
CREATE OPERATOR >= (
    PROCEDURE = hyperloglog_greater_than_equal,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '>=' , NEGATOR = '<',
    RESTRICT = scalargtsel, JOIN = scalargtjoinsel
);

-- less than or equal to operator
CREATE OPERATOR <= (
    PROCEDURE = hyperloglog_less_than_equal,
    LEFTARG = hyperloglog_estimator,
    RIGHTARG = hyperloglog_estimator,
    COMMUTATOR = '<=' , NEGATOR = '>',
    RESTRICT = scalarltsel, JOIN = scalarltjoinsel
);

