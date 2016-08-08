CREATE TABLE mdt_test_emp (ename varchar(20),eno int,salary int,ssn int,gender char(1)) distributed by (ename,eno,gender);

CREATE UNIQUE INDEX mdt_eno_idx ON mdt_test_emp (eno);
CREATE INDEX mdt_gender_bmp_idx ON mdt_test_emp USING bitmap (gender);
CREATE INDEX mdt_lower_ename_idex  ON mdt_test_emp ((upper(ename))) WHERE upper(ename)='JIM';
CREATE  INDEX mdt_ename_idx ON mdt_test_emp  (ename) WITH (fillfactor =80);
CREATE  INDEX mdt_ename_idx1 ON mdt_test_emp  (ename) WITH (fillfactor =80);

ALTER INDEX mdt_gender_bmp_idx RENAME TO mdt_new_gender_bmp_idx;
ALTER INDEX mdt_ename_idx SET (fillfactor =100);
ALTER INDEX mdt_ename_idx1 SET (fillfactor =100);
ALTER INDEX mdt_ename_idx1 RESET (fillfactor) ;

drop index mdt_eno_idx;
drop index mdt_new_gender_bmp_idx;
drop index mdt_lower_ename_idex;
drop index mdt_ename_idx ;
drop index mdt_ename_idx1 ;
drop table mdt_test_emp;
