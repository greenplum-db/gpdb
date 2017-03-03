DROP DATABASE gptransfer_testdb1;
DROP DATABASE gptransfer_testdb3;
DROP DATABASE gptransfer_testdb4;
DROP DATABASE gptransfer_testdb5;

CREATE DATABASE gptransfer_testdb1;
CREATE DATABASE gptransfer_testdb3;
CREATE DATABASE gptransfer_testdb4;
CREATE DATABASE gptransfer_testdb5;

\c gptransfer_testdb1
CREATE TABLE t0(a int, b text) DISTRIBUTED BY (a);
INSERT INTO t0 SELECT generate_series, 'Test data' FROM generate_series(1, 100);
CREATE TABLE t1(a int, b text) DISTRIBUTED BY (a);
INSERT INTO t1 SELECT generate_series, 'More test data' FROM generate_series(101, 300);
CREATE TABLE t2(a int, b text) DISTRIBUTED BY (a);
INSERT INTO t2 SELECT generate_series, 'And some more test data' FROM generate_series(301, 600);

\c gptransfer_testdb3
CREATE TABLE t0(a int, b text) DISTRIBUTED BY (a);
INSERT INTO t0 SELECT generate_series, 'Test data' FROM generate_series(2101, 2800);
CREATE SCHEMA s1;
CREATE TABLE s1.t0(a int, b text) DISTRIBUTED BY (a);
INSERT INTO s1.t0 SELECT generate_series, 'Test data' FROM generate_series(2801, 3600);
CREATE SCHEMA s2;
CREATE TABLE s2.t0(a int, b text) DISTRIBUTED BY (a);
INSERT INTO s2.t0 SELECT generate_series, 'Test data' FROM generate_series(3601, 4500);

\c gptransfer_testdb4
CREATE TABLE empty_table(a int) DISTRIBUTED BY (a);
CREATE TABLE one_row_table(a int) DISTRIBUTED BY (a);
INSERT INTO one_row_table VALUES(1);

\c gptransfer_testdb5
CREATE TABLE wide_rows(a int, b text) DISTRIBUTED BY (a);
INSERT INTO wide_rows VALUES (1, repeat('x', 10*1024*1024-4));
INSERT INTO wide_rows SELECT generate_series, b FROM generate_series(2,10), wide_rows;
