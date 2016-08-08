--ALTER Schema name

          CREATE SCHEMA mdt_dept;
          CREATE TABLE mdt_dept.mdt_csc(
          stud_id int,
          stud_name varchar(20)
          ) DISTRIBUTED RANDOMLY;

          CREATE SCHEMA mdt_new_dept;
          ALTER TABLE mdt_dept.mdt_csc SET SCHEMA mdt_new_dept;

drop table mdt_new_dept.mdt_csc;
drop schema mdt_new_dept;
drop schema mdt_dept;

