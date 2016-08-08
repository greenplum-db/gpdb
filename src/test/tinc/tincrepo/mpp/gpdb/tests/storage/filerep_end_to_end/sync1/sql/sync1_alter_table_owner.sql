-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

--
-- SYNC1 ALTER TABLE OWNER
--
 CREATE TABLE sync1_alter_table_owner1 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into sync1_alter_table_owner1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from sync1_alter_table_owner1;


 CREATE TABLE sync1_alter_table_owner2 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into sync1_alter_table_owner2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from sync1_alter_table_owner2;

 CREATE TABLE sync1_alter_table_owner3 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into sync1_alter_table_owner3 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from sync1_alter_table_owner3;


 CREATE TABLE sync1_alter_table_owner4 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into sync1_alter_table_owner4 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from sync1_alter_table_owner4;


 CREATE TABLE sync1_alter_table_owner5 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into sync1_alter_table_owner5 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from sync1_alter_table_owner5;


 CREATE TABLE sync1_alter_table_owner6 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into sync1_alter_table_owner6 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from sync1_alter_table_owner6;


 CREATE TABLE sync1_alter_table_owner7 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into sync1_alter_table_owner7 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from sync1_alter_table_owner7;


 CREATE TABLE sync1_alter_table_owner8 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into sync1_alter_table_owner8 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from sync1_alter_table_owner8;



--
-- ALTER TABLE OWNER
--
 CREATE ROLE sync1_user_1;

 ALTER TABLE sync1_alter_table_owner1 OWNER TO sync1_user_1;

select count(*) from sync1_alter_table_owner1;

