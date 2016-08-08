CREATE OR REPLACE TEMP VIEW mdt_view1 AS SELECT 'Hello World'; 

CREATE TEMPORARY VIEW mdt_view2 AS SELECT text 'Hello World' AS hello;

CREATE OR REPLACE VIEW mdt_view3 AS SELECT 'Hello World';

CREATE VIEW mdt_view4 AS SELECT text 'Hello World' AS hello;

CREATE TABLE mdt_test_emp (ename varchar(20),eno int,salary int,ssn int,gender char(1)) distributed by (ename,eno,gender);
CREATE VIEW mdt_test_emp_view AS SELECT ename ,eno ,salary ,ssn FROM mdt_test_emp WHERE salary >5000;

drop view mdt_view3;
drop view mdt_view4;
drop view mdt_test_emp_view;
drop table mdt_test_emp;
