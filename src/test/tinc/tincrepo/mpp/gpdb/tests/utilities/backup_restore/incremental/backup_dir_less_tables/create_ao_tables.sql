-- @gucs gp_create_table_random_default_distribution=off
-- Drop users
Drop role ao_user1;

--Table in non-public schema

CREATE SCHEMA aoschema1;
CREATE TABLE aoschema1.ao_table3(
          stud_id int,
          stud_name varchar(20)
          ) with(appendonly=true) DISTRIBUTED by(stud_id);

insert into aoschema1.ao_table3 values ( 1,'ann');
insert into aoschema1.ao_table3 values ( 2,'ben');

CREATE TABLE ao_table20(
          stud_id int,
          stud_name varchar(20) default 'default'
          ) with(appendonly=true) DISTRIBUTED by(stud_id);

insert into ao_table20 values ( 1,'ann');
insert into ao_table20 values ( 2,'ben');

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
