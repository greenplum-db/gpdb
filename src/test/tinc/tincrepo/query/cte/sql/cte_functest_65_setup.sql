\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE if exists foo_ao;
DROP TABLE if exists bar_co;
CREATE TABLE foo_ao(a int, b int) WITH ( appendonly = true);
CREATE TABLE bar_co(c int, d int) WITH ( appendonly = true, orientation = column);

INSERT INTO foo_ao SELECT i as a, i+1 as b FROM generate_series(1,10)i;
INSERT INTO bar_co SELECT i as c, i+1 as d FROM generate_series(1,10)i;
