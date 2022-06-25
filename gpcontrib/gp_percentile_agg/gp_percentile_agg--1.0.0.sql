---------------------------------------------------------------------------
--
-- gp_percentile_agg.sql-
--    This file creates gp_percentila_cont() and gp_percentile_disc() for
--    percentile calculation but assumes data is passed in sorted.
--
--
-- Copyright (c) 2022-Present VMware, Inc. or its affiliates.
--
--
---------------------------------------------------------------------------

-- Look at gp_percentile_agg.c for the source.  Note we mark them IMMUTABLE,
-- since they always return the same outputs given the same inputs.

CREATE FUNCTION gp_percentile_cont_float8_transition(float8, float8, float8, int8, int8)
    RETURNS float8
AS '$libdir/gp_percentile_agg'
   LANGUAGE C IMMUTABLE;

CREATE FUNCTION gp_percentile_cont_interval_transition(interval, interval, float8, int8, int8)
    RETURNS interval
AS '$libdir/gp_percentile_agg'
   LANGUAGE C IMMUTABLE;

CREATE FUNCTION gp_percentile_cont_timestamp_transition(timestamp, timestamp, float8, int8, int8)
    RETURNS timestamp
AS '$libdir/gp_percentile_agg'
   LANGUAGE C IMMUTABLE;

CREATE FUNCTION gp_percentile_cont_timestamptz_transition(timestamptz, timestamptz, float8, int8, int8)
    RETURNS timestamptz
AS '$libdir/gp_percentile_agg'
   LANGUAGE C IMMUTABLE;

CREATE FUNCTION gp_percentile_disc_transition(anyelement, anyelement, float8, int8, int8)
    RETURNS anyelement
AS '$libdir/gp_percentile_agg'
   LANGUAGE C IMMUTABLE;

CREATE FUNCTION gp_percentile_final(anyelement)
    RETURNS anyelement
AS '$libdir/gp_percentile_agg'
   LANGUAGE C IMMUTABLE;

-- Creating aggregate functions
CREATE AGGREGATE gp_percentile_cont(float8, float8, int8, int8)
(
     SFUNC = gp_percentile_cont_float8_transition,
     STYPE = float8,
     FINALFUNC = gp_percentile_final
);

CREATE AGGREGATE gp_percentile_cont(interval, float8, int8, int8)
(
     SFUNC = gp_percentile_cont_interval_transition,
     STYPE = interval,
     FINALFUNC = gp_percentile_final
);

CREATE AGGREGATE gp_percentile_cont(timestamp, float8, int8, int8)
(
     SFUNC = gp_percentile_cont_timestamp_transition,
     STYPE = timestamp,
     FINALFUNC = gp_percentile_final
);

CREATE AGGREGATE gp_percentile_cont(timestamptz, float8, int8, int8)
(
     SFUNC = gp_percentile_cont_timestamptz_transition,
     STYPE = timestamptz,
     FINALFUNC = gp_percentile_final
);

CREATE AGGREGATE gp_percentile_disc(anyelement, float8, int8, int8)
(
     SFUNC = gp_percentile_disc_transition,
     STYPE = anyelement,
     FINALFUNC = gp_percentile_final
);
