-- @gucs gp_create_table_random_default_distribution=off
-- Create heap tables 

CREATE TABLE heap_table1_1(
          col_text text,
          col_numeric numeric
          ) DISTRIBUTED RANDOMLY;

insert into heap_table1_1 values ('0_zero',0);
insert into heap_table1_1 values ('1_one',1);
insert into heap_table1_1 values ('2_two',2);

--Table in non-public schema

CREATE SCHEMA schema1_1;
CREATE TABLE schema1_1.heap_table2_1(
          stud_id int,
          stud_name varchar(20)
          ) DISTRIBUTED by(stud_id);

insert into schema1_1.heap_table2_1 values ( 1,'ann');
insert into schema1_1.heap_table2_1 values ( 1,'ben');
insert into schema1_1.heap_table2_1 values ( 3,'sam');
insert into schema1_1.heap_table2_1 select i,i||'_'||repeat('text',3) from generate_series(4,100)i;
