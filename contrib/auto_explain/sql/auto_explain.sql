-- start_matchsubs
-- m/^LOG.*\"Falling/
-- s/^LOG.*\"Falling/\"Falling/
-- end_matchsubs

CREATE SCHEMA auto_explain_test;
CREATE TABLE auto_explain_test.t1(a int);
INSERT INTO auto_explain_test.t1 VALUES(generate_series(0, 1000));
ANALYZE auto_explain_test.t1;
CREATE TABLE auto_explain_test.t2(b int);
INSERT INTO auto_explain_test.t2 VALUES(generate_series(0, 1000));
ANALYZE auto_explain_test.t2;

SET enable_nestloop = ON;
SET CLIENT_MIN_MESSAGES = LOG;
LOAD 'auto_explain';
SET auto_explain.log_analyze = TRUE;
SET auto_explain.log_min_duration = 0;
SET auto_explain.log_buffers = FALSE;
SET auto_explain.log_triggers = TRUE;
SET auto_explain.log_nested_statements = FALSE;
SET auto_explain.log_timing = FALSE;
SET auto_explain.log_verbose = FALSE;

SELECT relname FROM pg_class WHERE relname='pg_class';
SELECT count(*) FROM auto_explain_test.t1, auto_explain_test.t2;

SET auto_explain.log_min_duration = 1;
SET auto_explain.log_triggers = FALSE;
SET auto_explain.log_verbose = TRUE;

-- this select should not dump execution plan
SELECT relname FROM pg_class WHERE relname='pg_class';
-- this select should also dump plan, since it takes too much time to run
SELECT count(*) FROM auto_explain_test.t1, auto_explain_test.t2;

-- clean jobs
DROP TABLE auto_explain_test.t1;
DROP TABLE auto_explain_test.t2;
DROP SCHEMA auto_explain_test;
