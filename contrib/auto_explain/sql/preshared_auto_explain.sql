-- need to start cluster with gpconfig -c shared_preload_libraries -v 'auto_explain' before this test

SET enable_nestloop = ON;
SET CLIENT_MIN_MESSAGES = LOG;
SET auto_explain.log_min_duration = 0;

CREATE TABLE t1 as select generate_series(1, 10*1000*1000);
CREATE TABLE t2 (i int, j int);
SELECT t3.i, t4.j FROM t2 as t3 join t2 as t4 on t3.i = t4.j;
DROP TABLE t1;
DROP TABLE t2;

SET auto_explain.log_min_duration = 1;

CREATE TABLE t1 as select generate_series(1, 10*1000*1000);
CREATE TABLE t2 (i int, j int);
SELECT t3.i, t4.j FROM t2 as t3 join t2 as t4 on t3.i = t4.j;
DROP TABLE t1;
DROP TABLE t2;
