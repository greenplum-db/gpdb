DROP DATABASE IF EXISTS gptransfer_filespace_test;

DROP TABLESPACE IF EXISTS ts_test;

CREATE TABLESPACE ts_test FILESPACE fs; 

CREATE DATABASE gptransfer_filespace_test TABLESPACE ts_test;

\c gptransfer_filespace_test

CREATE TABLE t2 (i int) TABLESPACE ts_test;

INSERT INTO t2 VALUES (1);
INSERT INTO t2 VALUES (2);
