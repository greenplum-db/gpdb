-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

--
-- RESYNC ALTER TABLE OWNER
--
 CREATE TABLE resync_alter_table_owner1 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into resync_alter_table_owner1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from resync_alter_table_owner1;


 CREATE TABLE resync_alter_table_owner2 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into resync_alter_table_owner2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from resync_alter_table_owner2;

 CREATE TABLE resync_alter_table_owner3 (
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric
 )DISTRIBUTED RANDOMLY;

 insert into resync_alter_table_owner3 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',5),i from generate_series(1,100)i;

select count(*) from resync_alter_table_owner3;


--
-- ALTER TABLE OWNER
--
 CREATE ROLE resync_user_1;

 ALTER TABLE sync1_alter_table_owner6 OWNER TO resync_user_1;
select count(*) from  sync1_alter_table_owner6;

 ALTER TABLE ck_sync1_alter_table_owner5 OWNER TO resync_user_1;
select count(*) from  ck_sync1_alter_table_owner5;

 ALTER TABLE ct_alter_table_owner3 OWNER TO resync_user_1;
select count(*) from  ct_alter_table_owner3;

 ALTER TABLE resync_alter_table_owner1 OWNER TO resync_user_1;
select count(*) from resync_alter_table_owner1;
