-- @gucs gp_create_table_random_default_distribution=off
-- Drop users
Drop role ao_user1;

-- Create AO tables 

CREATE TABLE ao_table1(
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric,
          int_col int4 NOT NULL,
          float_col float4,
          int_array_col int[],
          before_rename_col int4,
          change_datatype_col numeric,
          a_ts_without timestamp without time zone,
          b_ts_with timestamp with time zone,
          date_column date ,
          col_set_default numeric) with(appendonly=true) DISTRIBUTED RANDOMLY;

insert into ao_table1 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
insert into ao_table1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
insert into ao_table1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);

--Table in non-public schema

CREATE SCHEMA aoschema1;
CREATE TABLE aoschema1.ao_table3(
          stud_id int,
          stud_name varchar(20)
          ) with(appendonly=true) DISTRIBUTED by(stud_id);

insert into aoschema1.ao_table3 values ( 1,'ann');
insert into aoschema1.ao_table3 values ( 2,'ben');

-- Parent and child table for inheritance test

CREATE TABLE ao_table_parent (
  text_col text,
  bigint_col bigint,
  char_vary_col character varying(30),
  numeric_col numeric
  ) with(appendonly= true) DISTRIBUTED RANDOMLY;

insert into ao_table_parent values ('0_zero', 0, '0_zero', 0); 
insert into ao_table_parent values ('1_zero', 1, '1_zero', 1); 
insert into ao_table_parent values ('2_zero', 2, '2_zero', 2); 

CREATE TABLE ao_table_child(
  text_col text,
  bigint_col bigint,
  char_vary_col character varying(30),
  numeric_col numeric
  ) with(appendonly= true)  DISTRIBUTED RANDOMLY;

insert into ao_table_child values ('3_zero', 3, '3_zero', 3); 

--Create Role
Drop role ao_user1;
CREATE ROLE ao_user1;

Create table ao_table4 with(appendonly=true) as select * from ao_table2;
