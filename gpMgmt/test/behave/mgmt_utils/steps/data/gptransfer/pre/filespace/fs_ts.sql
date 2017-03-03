-- This require a filespace 'fs' created before running this test
-- refer to the behave test case which creates the filespace

CREATE TABLESPACE ts filespace fs; 

CREATE DATABASE db_ts TABLESPACE ts; 

\c db_ts;

CREATE TABLE tbl2_ts (
        id int,
        gender char
) TABLESPACE ts; 

INSERT INTO tbl2_ts VALUES (1,'M');
INSERT INTO tbl2_ts VALUES (2,'F');
