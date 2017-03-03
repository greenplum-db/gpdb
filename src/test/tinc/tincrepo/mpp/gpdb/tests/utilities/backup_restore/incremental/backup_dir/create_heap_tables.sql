-- @gucs gp_create_table_random_default_distribution=off
-- Create heap tables 

CREATE TABLE heap_table1(
          col_text text,
          col_numeric numeric
          ) DISTRIBUTED RANDOMLY;

insert into heap_table1 values ('0_zero',0);
insert into heap_table1 values ('1_one',1);
insert into heap_table1 values ('2_two',2);

--Table in non-public schema

CREATE SCHEMA schema1;
CREATE TABLE schema1.heap_table2(
          stud_id int,
          stud_name varchar(20)
          ) DISTRIBUTED by(stud_id);

insert into schema1.heap_table2 values ( 1,'ann');
insert into schema1.heap_table2 values ( 1,'ben');
insert into schema1.heap_table2 values ( 3,'sam');
insert into schema1.heap_table2 select i,i||'_'||repeat('text',3) from generate_series(4,100)i;

CREATE TABLE heap_table3(
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
          date_column date,
          col_set_default numeric)DISTRIBUTED RANDOMLY;

insert into heap_table3 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
insert into heap_table3 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
insert into heap_table3 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);


CREATE TABLE heap_table4(
          col_text text,
          col_numeric numeric
          ) DISTRIBUTED RANDOMLY;

insert into heap_table4 values ('0_zero',0);
insert into heap_table4 values ('1_one',1);
insert into heap_table4 values ('2_two',2);

create table heap_part12 ( a int, b text) partition by range(a) (start(1) end(9) every(5));
