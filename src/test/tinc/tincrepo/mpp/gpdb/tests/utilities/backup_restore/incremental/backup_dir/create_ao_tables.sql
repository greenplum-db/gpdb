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

CREATE TABLE ao_table2(
          col_text text,
          col_numeric numeric
          CONSTRAINT tbl_chk_con1 CHECK (col_numeric < 250)
          ) with(appendonly=true) DISTRIBUTED by(col_text);

insert into ao_table2 values ('0_zero',0);
insert into ao_table2 values ('1_one',1);
insert into ao_table2 values ('2_two',2);
insert into ao_table2 select i||'_'||repeat('text',100),i from generate_series(1,100)i;

--Table in non-public schema

CREATE SCHEMA aoschema1;
CREATE TABLE aoschema1.ao_table3(
          stud_id int,
          stud_name varchar(20)
          ) with(appendonly=true) DISTRIBUTED by(stud_id);

insert into aoschema1.ao_table3 values ( 1,'ann');
insert into aoschema1.ao_table3 values ( 2,'ben');

Create table ao_table4 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table5 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table6 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table7 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table8 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table9 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table10 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table11 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table12 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table13 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table14 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table15 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create index ao_table15_idx on ao_table15(bigint_col);
Create table ao_table16 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create index ao_table16_idx on ao_table16(bigint_col);
Create table ao_table17 with(appendonly=true, OIDS=TRUE) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table18 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table19 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);
Create table ao_table20 with(appendonly=true) as select * from ao_table1 DISTRIBUTED BY (text_col);

CREATE TABLE ao_table21(
          stud_id int,
          stud_name varchar(20) default 'default'
          ) with(appendonly=true) DISTRIBUTED by(stud_id);

insert into ao_table21 values ( 1,'ann');
insert into ao_table21 values ( 2,'ben');

CREATE TABLE aoschema1.ao_table22(
          stud_id int,
          stud_name varchar(20)
          ) with(appendonly=true) DISTRIBUTED by(stud_id);

insert into aoschema1.ao_table22 values(1, 'data1');
insert into aoschema1.ao_table22 values(2, 'data2');

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

Create table ao_table23 with(appendonly=true) as select * from ao_table2 DISTRIBUTED BY (col_text);
Create table ao_table24 with(appendonly=true) as select * from ao_table2 DISTRIBUTED BY (col_text);
Create table ao_table25 with(appendonly=true) as select * from ao_table2 DISTRIBUTED BY (col_text);
