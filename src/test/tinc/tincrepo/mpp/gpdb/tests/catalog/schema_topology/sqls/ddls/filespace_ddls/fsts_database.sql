-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology

-- Create Roles   
   CREATE ROLE sch_fsts_db_owner1;

-- Create Database
   CREATE DATABASE sch_fsts_db_name1 WITH OWNER = sch_fsts_db_owner1 TEMPLATE =template1 ENCODING='utf8'  CONNECTION LIMIT= 2 TABLESPACE = ts_sch1;
   
-- Alter Database   
   ALTER DATABASE sch_fsts_db_name1 WITH  CONNECTION LIMIT 3;
-- Alter Database to new tablespace is not supported  
   ALTER DATABASE sch_fsts_db_name1 set TABLESPACE = ts_sch3;


   set default_tablespace='ts_sch3';
--Create Table   
  CREATE TABLE sch_fsts_test_part1 (id int, name text,rank int, year date, gender  char(1)) tablespace ts_sch3 DISTRIBUTED BY (id, gender, year)
      partition by list (gender) subpartition by range (year) subpartition template (start (date '2001-01-01')) 
      (values ('M'),values ('F'));

-- Add default partition
   alter table sch_fsts_test_part1 add default partition default_part;

-- Drop Default Partition
   alter table sch_fsts_test_part1 DROP default partition if exists;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_test_part1 ;

-- Alter the table to new table space 
   alter table sch_fsts_test_part1 set tablespace ts_sch2;

-- Insert few records into the table
   insert into sch_fsts_test_part1 values (1,'ann',1,'2001-01-01','F');
   insert into sch_fsts_test_part1 values (2,'ben',2,'2002-01-01','M');
   insert into sch_fsts_test_part1 values (3,'leni',3,'2003-01-01','F');
   insert into sch_fsts_test_part1 values (4,'sam',4,'2003-01-01','M');

-- Alter the table set distributed by 
   Alter table sch_fsts_test_part1 set with ( reorganize='true') distributed randomly;

-- select from the Table
   select * from sch_fsts_test_part1;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_test_part1 ;

   vacuum analyze ;
