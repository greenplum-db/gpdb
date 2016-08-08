-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology

\c db_test_bed
CREATE TEMPORARY SEQUENCE  db_seq1 START WITH 101;
CREATE TEMP SEQUENCE  db_seq2 START 101;
CREATE SEQUENCE db_seq3  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100;
CREATE SEQUENCE db_seq4  INCREMENT BY 2 NO MINVALUE  NO MAXVALUE ;
CREATE SEQUENCE db_seq5  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE db_seq6  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  NO CYCLE;
CREATE SEQUENCE db_seq7 START 101 OWNED BY NONE;
CREATE TABLE test_tbl ( col1 int, col2 int) DISTRIBUTED RANDOMLY;
INSERT INTO test_tbl values (generate_series(1,100),generate_series(1,100));
CREATE SEQUENCE db_seq8 START 101 OWNED BY test_tbl.col1;
ALTER TABLE test_tbl DROP COLUMN col1;

ALTER SEQUENCE db_seq1 RESTART WITH 100;
ALTER SEQUENCE db_seq2 INCREMENT BY 2 MINVALUE 101 MAXVALUE  400  CACHE 100 CYCLE;
ALTER SEQUENCE db_seq3  INCREMENT BY 2 NO MINVALUE  NO MAXVALUE;
ALTER SEQUENCE db_seq4 INCREMENT BY 2 MINVALUE 1 MAXVALUE  100;
ALTER SEQUENCE db_seq5 NO CYCLE;
CREATE SCHEMA db_schema9;
ALTER SEQUENCE db_seq6 SET SCHEMA db_schema9;
ALTER SEQUENCE db_seq7  OWNED BY test_tbl.col2;
ALTER SEQUENCE db_seq7  OWNED BY NONE;
