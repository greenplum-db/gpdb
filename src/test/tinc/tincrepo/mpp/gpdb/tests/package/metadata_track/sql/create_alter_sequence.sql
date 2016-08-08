CREATE TEMPORARY SEQUENCE  mdt_db_seq1 START WITH 101;
CREATE SEQUENCE  mdt_db_seq9 START WITH 101;
CREATE TEMP SEQUENCE  mdt_db_seq2 START 101;
CREATE SEQUENCE  mdt_db_seq10 START 101;
CREATE SEQUENCE mdt_db_seq3  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100;
CREATE SEQUENCE mdt_db_seq4  INCREMENT BY 2 NO MINVALUE  NO MAXVALUE ;
CREATE SEQUENCE mdt_db_seq5  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  CACHE 100 CYCLE;
CREATE SEQUENCE mdt_db_seq6  INCREMENT BY 2 MINVALUE 1 MAXVALUE  100  NO CYCLE;
CREATE SEQUENCE mdt_db_seq7 START 101 OWNED BY NONE;
CREATE TABLE mdt_test_tbl ( col1 int, col2 int) DISTRIBUTED RANDOMLY;
INSERT INTO mdt_test_tbl values (generate_series(1,100),generate_series(1,100));
CREATE SEQUENCE mdt_db_seq8 START 101 OWNED BY mdt_test_tbl.col1;



ALTER SEQUENCE mdt_db_seq1 RESTART WITH 100;
ALTER SEQUENCE mdt_db_seq9 RESTART WITH 100;
ALTER SEQUENCE mdt_db_seq2 INCREMENT BY 2 MINVALUE 101 MAXVALUE  400  CACHE 100 CYCLE;
ALTER SEQUENCE mdt_db_seq10 INCREMENT BY 2 MINVALUE 101 MAXVALUE  400  CACHE 100 CYCLE;
ALTER SEQUENCE mdt_db_seq3  INCREMENT BY 2 NO MINVALUE  NO MAXVALUE;
ALTER SEQUENCE mdt_db_seq4 INCREMENT BY 2 MINVALUE 1 MAXVALUE  100;
ALTER SEQUENCE mdt_db_seq5 NO CYCLE;
CREATE SCHEMA mdt_db_schema9;
ALTER SEQUENCE mdt_db_seq6 SET SCHEMA mdt_db_schema9;
ALTER SEQUENCE mdt_db_seq7  OWNED BY mdt_test_tbl.col2;
ALTER SEQUENCE mdt_db_seq7  OWNED BY NONE;

drop sequence if exists mdt_db_seq1;
drop sequence mdt_db_seq2 cascade;
drop sequence mdt_db_seq3 restrict;
drop sequence mdt_db_seq4;
drop sequence mdt_db_seq5;
drop sequence mdt_db_schema9.mdt_db_seq6;
drop sequence mdt_db_seq7;
drop sequence mdt_db_seq8;
drop schema mdt_db_schema9 ;
drop table mdt_test_tbl;
drop sequence mdt_db_seq9;
drop sequence mdt_db_seq10;
