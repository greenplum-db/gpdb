-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description ddl/dml to create, alter filespaces

-- Alter filespace
   alter filespace cdbfast_fs_sch1 rename to new_cdbfast_fs_sch1;
   alter filespace new_cdbfast_fs_sch1 rename to cdbfast_fs_sch1;
   
-- Alter table constraint

   CREATE TABLE sch_fsts_films (
          code char(5),
          title varchar(40),
          did integer,
          date_prod date,
          kind varchar(10),
          len interval hour to minute,
          CONSTRAINT production UNIQUE(date_prod)
          ) tablespace ts_sch2 ;
-- Insert few records into the table
   insert into sch_fsts_films values ('aaa','Heavenly Life',1,'2008-03-03','good','2 hr 30 mins');
   insert into sch_fsts_films values ('bbb','Beautiful Mind',2,'2007-05-05','good','1 hr 30 mins');
   insert into sch_fsts_films values ('ccc','Wonderful Earth',3,'2009-03-03','good','2 hr 15 mins');

-- select from the Table
   select * from sch_fsts_films;

-- Alter Table Add Unique Constraint
   ALTER TABLE sch_fsts_films ADD UNIQUE(date_prod);

-- Alter Table Drop Constraint
   ALTER TABLE sch_fsts_films DROP CONSTRAINT production RESTRICT;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_films;

-- Alter the table to new table space 
   ALTER TABLE sch_fsts_films set tablespace ts_sch6;

-- Insert few records into the table
   insert into sch_fsts_films values ('aaaa','Heavenly Life',1,'2006-03-03','good','2 hr 30 mins');
   insert into sch_fsts_films values ('bbbb','Beautiful Mind',2,'2005-05-05','good','1 hr 30 mins');
   insert into sch_fsts_films values ('cccc','Wonderful Earth',3,'2004-03-03','good','2 hr 15 mins');

-- select from the Table
   select * from sch_fsts_films;

-- Truncate table
   truncate sch_fsts_films;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_films;

       

-- Alter table inherit
   CREATE TABLE sch_fsts_parent_table (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) tablespace ts_sch2 DISTRIBUTED RANDOMLY;

   CREATE TABLE sch_fsts_child_table(
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) tablespace ts_sch3 DISTRIBUTED RANDOMLY;
          
-- Insert few records into the table
   insert into sch_fsts_parent_table values ('0_zero', 0, '0_zero', 0);
   insert into sch_fsts_parent_table values ('1_zero', 1, '1_zero', 1);
   insert into sch_fsts_parent_table values ('2_zero', 2, '2_zero', 2);
   insert into sch_fsts_child_table values ('3_zero', 3, '3_zero', 3);

-- select from the Table
   select * from sch_fsts_child_table;

-- Inherit from Parent Table
   ALTER TABLE sch_fsts_child_table INHERIT sch_fsts_parent_table;

-- No Inherit from Parent Table
   ALTER TABLE sch_fsts_child_table NO INHERIT sch_fsts_parent_table;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_child_table;

-- Alter the table to new table space 
   ALTER TABLE sch_fsts_child_table set tablespace ts_sch5;

-- Insert few records into the table
   insert into sch_fsts_parent_table values ('0_zero', 0, '0_zero', 0);
   insert into sch_fsts_parent_table values ('1_zero', 1, '1_zero', 1);
   insert into sch_fsts_parent_table values ('2_zero', 2, '2_zero', 2);
   insert into sch_fsts_child_table values ('3_zero', 3, '3_zero', 3);

-- Alter the table set distributed by 
   Alter table sch_fsts_child_table set with ( reorganize='true') distributed by (numeric_col);

-- select from the Table
   select * from sch_fsts_child_table;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_child_table;

       

-- Alter Partition table exchange range
   CREATE TABLE fsts_part_range (
        unique1         int4,
        unique2         int4
        )  tablespace ts_sch3 partition by range (unique1)
        ( partition aa start (0) end (500) every (100), default partition default_part );
   CREATE TABLE fsts_part_range_A6 (
        unique1         int4,
        unique2         int4) tablespace ts_sch3 ;

-- Insert few records into the table
   insert into fsts_part_range values ( generate_series(5,50),generate_series(15,60));
   insert into fsts_part_range_A6 values ( generate_series(1,10),generate_series(21,30));

-- select from the Table
   select count(*) from fsts_part_range;

-- ALTER PARTITION TABLE EXCHANGE PARTITION RANGE
   alter table fsts_part_range exchange partition for (rank(1)) with table fsts_part_range_A6;

-- Truncate Table
   truncate fsts_part_range_A6;  

-- Vacuum analyze the table
   vacuum analyze fsts_part_range_A6 ;

-- Alter the table to new table space 
   alter table fsts_part_range set tablespace ts_sch6;

-- Insert few records into the table
   insert into fsts_part_range values ( generate_series(5,50),generate_series(15,60));
   insert into fsts_part_range_A6 values ( generate_series(1,10),generate_series(21,30));

-- Alter the table set distributed by 
   Alter table fsts_part_range set with ( reorganize='true') distributed by (unique2); 

-- select from the Table
   select count(*) from fsts_part_range;

-- Vacuum analyze the table
   vacuum analyze fsts_part_range_A6 ;


