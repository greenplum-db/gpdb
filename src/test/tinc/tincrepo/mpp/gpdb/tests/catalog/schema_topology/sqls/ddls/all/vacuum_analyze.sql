-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Vaccum analysis

 create database db_tobe_vacuum;

\c db_tobe_vacuum

set time zone PST8PDT;

CREATE TABLE test_add_drop_rename_column_change_datatype(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) distributed randomly;

insert into test_add_drop_rename_column_change_datatype values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into test_add_drop_rename_column_change_datatype values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into test_add_drop_rename_column_change_datatype values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into test_add_drop_rename_column_change_datatype select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



ALTER TABLE test_add_drop_rename_column_change_datatype ADD COLUMN added_col character varying(30);
ALTER TABLE test_add_drop_rename_column_change_datatype DROP COLUMN drop_col ;
ALTER TABLE test_add_drop_rename_column_change_datatype RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE test_add_drop_rename_column_change_datatype ALTER COLUMN change_datatype_col TYPE int4;

CREATE TABLE test_drop_constraint_default (
col_with_default numeric DEFAULT 0,
col_with_default_drop_default character varying(30) DEFAULT 'test1',
col_with_constraint numeric UNIQUE
) DISTRIBUTED BY (col_with_constraint);

insert into test_drop_constraint_default(col_with_default_drop_default,col_with_constraint) values('0_zero',0);
insert into test_drop_constraint_default values(11,'1_zero',1);
insert into test_drop_constraint_default (col_with_default,col_with_constraint)values (33,3);

ALTER TABLE test_drop_constraint_default ALTER COLUMN col_with_default_drop_default DROP DEFAULT;
insert into test_drop_constraint_default (col_with_default,col_with_constraint)values(77,7);
ALTER TABLE test_drop_constraint_default ADD CONSTRAINT test_unique UNIQUE (col_with_constraint);
ALTER TABLE test_drop_constraint_default DROP CONSTRAINT test_unique;

ALTER TABLE test_drop_constraint_default DROP COLUMN col_with_constraint;

VACUUM;

create database db_tobe_vacuum_analyze;

\c db_tobe_vacuum_analyze

CREATE TABLE test_add_drop_rename_column_change_datatype(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) distributed randomly;

insert into test_add_drop_rename_column_change_datatype values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into test_add_drop_rename_column_change_datatype values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into test_add_drop_rename_column_change_datatype values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into test_add_drop_rename_column_change_datatype select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

ALTER TABLE test_add_drop_rename_column_change_datatype ADD COLUMN added_col character varying(30);
ALTER TABLE test_add_drop_rename_column_change_datatype DROP COLUMN drop_col ;
ALTER TABLE test_add_drop_rename_column_change_datatype RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE test_add_drop_rename_column_change_datatype ALTER COLUMN change_datatype_col TYPE int4;

CREATE TABLE test_drop_constraint_default (
col_with_default numeric DEFAULT 0,
col_with_default_drop_default character varying(30) DEFAULT 'test1',
col_with_constraint numeric UNIQUE
) DISTRIBUTED BY (col_with_constraint);

insert into test_drop_constraint_default(col_with_default_drop_default,col_with_constraint) values('0_zero',0);
insert into test_drop_constraint_default values(11,'1_zero',1);
insert into test_drop_constraint_default (col_with_default,col_with_constraint)values (33,3);

ALTER TABLE test_drop_constraint_default ALTER COLUMN col_with_default_drop_default DROP DEFAULT;
insert into test_drop_constraint_default (col_with_default,col_with_constraint)values(77,7);
ALTER TABLE test_drop_constraint_default ADD CONSTRAINT test_unique UNIQUE (col_with_constraint);
ALTER TABLE test_drop_constraint_default DROP CONSTRAINT test_unique;

ALTER TABLE test_drop_constraint_default DROP COLUMN col_with_constraint;

VACUUM ANALYZE;

create database db_test;

\c db_test

create table table1 (col1 int,col2 int) distributed randomly;

insert into table1 values (1,1);
insert into table1 values (2,2);
insert into table1 values (3,3);
insert into table1 values (4,4);
insert into table1 values (generate_series(5,100),generate_series(5,100));

VACUUM ANALYZE table1(col1);

create table table2 (col1 int,col2 int) distributed randomly;

insert into table2 values (1,1);
insert into table2 values (2,2);
insert into table2 values (3,3);
insert into table2 values (4,4);
insert into table2 values (generate_series(5,100),generate_series(5,100));

create index clusterindex on table2(col1);
CLUSTER clusterindex on table2;

create table table_tobe_truncate ( i int, j text) distributed randomly;
insert into table_tobe_truncate values (1,'1_test');
insert into table_tobe_truncate values (2,'2_test');
insert into table_tobe_truncate values (3,'3_test');
insert into table_tobe_truncate values (4,'4_test');
insert into table_tobe_truncate values (5,'5_test');
insert into table_tobe_truncate select i,i||'_'||repeat('text',100) from generate_series(6,100)i;


TRUNCATE table_tobe_truncate;

CREATE TABLE table3 (
col_with_default numeric DEFAULT 0,
col_with_default_drop_default character varying(30) DEFAULT 'test1',
col_with_constraint numeric UNIQUE
) DISTRIBUTED BY (col_with_constraint);

insert into table3(col_with_default_drop_default,col_with_constraint) values('0_zero',0);
insert into table3 values(11,'1_zero',1);
insert into table3 (col_with_default,col_with_constraint)values (33,3);

CREATE TABLE table4 AS SELECT * FROM table3;

create table bm_test (i int, j int);
insert into bm_test values (0, 0), (0, 0), (0, 1), (1,0), (1,0), (1,1);
create index bm_test_j on bm_test using bitmap(j);
delete from bm_test where j =1;
insert into bm_test values (0, 0), (1,0);
insert into bm_test values (generate_series(2,100),generate_series(2,100));

REINDEX index "public"."bm_test_j";

create table table_tobe_vacuum ( i int, j text) distributed randomly;
insert into table_tobe_vacuum values (1,'1_test');
insert into table_tobe_vacuum values (2,'2_test');
insert into table_tobe_vacuum values (3,'3_test');
insert into table_tobe_vacuum values (4,'4_test');
insert into table_tobe_vacuum values (5,'5_test');
insert into table_tobe_vacuum select i,i||'_'||repeat('text',100) from generate_series(6,100)i;

VACUUM table_tobe_vacuum;



