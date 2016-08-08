-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description create and alter indexes

\c db_test_bed
CREATE TABLE test_emp (ename varchar(20),eno int,salary int,ssn int,gender char(1)) distributed by (ename,eno,gender);

CREATE UNIQUE INDEX eno_idx ON test_emp (eno);
CREATE INDEX gender_bmp_idx ON test_emp USING bitmap (gender);
CREATE INDEX lower_ename_idex  ON test_emp ((upper(ename))) WHERE upper(ename)='JIM';
CREATE  INDEX ename_idx ON test_emp  (ename) WITH (fillfactor =80);

ALTER INDEX gender_bmp_idx RENAME TO new_gender_bmp_idx;
ALTER INDEX ename_idx SET (fillfactor =100);
ALTER INDEX ename_idx RESET (fillfactor) ;
