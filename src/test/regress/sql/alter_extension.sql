-- Creating extension to test alter extension
-- Assume: pageinspect is shipped by default
CREATE EXTENSION pageinspect;

CREATE AGGREGATE example_agg(int4) (
    SFUNC = int4larger,
    STYPE = int4
);

ALTER EXTENSION pageinspect ADD AGGREGATE example_agg(int4);
ALTER EXTENSION pageinspect DROP AGGREGATE example_agg(int4);

DROP EXTENSION pageinspect;

-- Test creating an extension that already exists. Nothing too exciting about
-- it, but let's keep up the test coverage.
CREATE EXTENSION gp_inject_fault;
