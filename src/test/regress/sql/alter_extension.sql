CREATE AGGREGATE example_agg(int4) (
    SFUNC = int4larger,
    STYPE = int4
);

ALTER EXTENSION gp_inject_fault ADD AGGREGATE example_agg(int4);
ALTER EXTENSION gp_inject_fault DROP AGGREGATE example_agg(int4);
