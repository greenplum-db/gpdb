-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

--
-- CT ALTER TABLE OWNER
--
 CREATE TABLE ct_alter_table_owner1 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into ct_alter_table_owner1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from ct_alter_table_owner1;


 CREATE TABLE ct_alter_table_owner2 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into ct_alter_table_owner2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from ct_alter_table_owner2;

 CREATE TABLE ct_alter_table_owner3 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into ct_alter_table_owner3 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from ct_alter_table_owner3;


 CREATE TABLE ct_alter_table_owner4 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into ct_alter_table_owner4 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from ct_alter_table_owner4;


 CREATE TABLE ct_alter_table_owner5 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into ct_alter_table_owner5 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from ct_alter_table_owner5;


--
-- ALTER TABLE OWNER
--
 CREATE ROLE ct_user_1;

 ALTER TABLE sync1_alter_table_owner4 OWNER TO ct_user_1;
select count(*) from  sync1_alter_table_owner4;

 ALTER TABLE ck_sync1_alter_table_owner3 OWNER TO ct_user_1;
select count(*) from  ck_sync1_alter_table_owner3;

 ALTER TABLE ct_alter_table_owner1 OWNER TO ct_user_1;
select count(*) from ct_alter_table_owner1;





