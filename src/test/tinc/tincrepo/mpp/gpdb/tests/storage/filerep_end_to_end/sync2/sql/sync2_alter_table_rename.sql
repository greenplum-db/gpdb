-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC2 RENAME TABLE
--
--
-- HEAP TABLE
--
CREATE TABLE sync2_heap_alter_table_rename1(
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

INSERT INTO sync2_heap_alter_table_rename1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{1}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1,10)i;

CREATE TABLE sync2_heap_alter_table_rename2(
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

INSERT INTO sync2_heap_alter_table_rename2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{1}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1,10)i;


--
--
-- ALTER TABLE Rename HEAP Table
--
--
ALTER TABLE sync1_heap_alter_table_rename7 RENAME TO sync1_heap_alter_table_newname7;
ALTER TABLE ck_sync1_heap_alter_table_rename6 RENAME TO ck_sync1_heap_alter_table_newname6;
ALTER TABLE ct_heap_alter_table_rename4 RENAME TO ct_heap_alter_table_newname4;
ALTER TABLE resync_heap_alter_table_rename2 RENAME TO resync_heap_alter_table_newname2;
ALTER TABLE sync2_heap_alter_table_rename1 RENAME TO sync2_heap_alter_table_newname1;

--
-- AO TABLE
--
CREATE TABLE sync2_ao_alter_table_rename1(
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
date_column date)  with ( appendonly='true') distributed randomly;

INSERT INTO sync2_ao_alter_table_rename1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{1}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1,10)i;

CREATE TABLE sync2_ao_alter_table_rename2(
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
date_column date)   with ( appendonly='true') distributed randomly;

INSERT INTO sync2_ao_alter_table_rename2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{1}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1,10)i;

--
--
-- ALTER TABLE Rename AO Table
--
--
ALTER TABLE sync1_ao_alter_table_rename7 RENAME TO sync1_ao_alter_table_newname7;
ALTER TABLE ck_sync1_ao_alter_table_rename6 RENAME TO ck_sync1_ao_alter_table_newname6;
ALTER TABLE ct_ao_alter_table_rename4 RENAME TO ct_ao_alter_table_newname4;
ALTER TABLE resync_ao_alter_table_rename2 RENAME TO resync_ao_alter_table_newname2;
ALTER TABLE sync2_ao_alter_table_rename1 RENAME TO sync2_ao_alter_table_newname1;

--
-- CO TABLE
--

CREATE TABLE sync2_co_alter_table_rename1(
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
date_column date)  with ( appendonly='true', orientation='column') distributed randomly;

INSERT INTO sync2_co_alter_table_rename1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{1}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1,10)i;

CREATE TABLE sync2_co_alter_table_rename2(
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
date_column date)  with ( appendonly='true', orientation='column') distributed randomly;

INSERT INTO sync2_co_alter_table_rename2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{1}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1,10)i;

--
--
-- ALTER TABLE Rename CO Table
--
--
ALTER TABLE sync1_co_alter_table_rename7 RENAME TO sync1_co_alter_table_newname7;
ALTER TABLE ck_sync1_co_alter_table_rename6 RENAME TO ck_sync1_co_alter_table_newname6;
ALTER TABLE ct_co_alter_table_rename4 RENAME TO ct_co_alter_table_newname4;
ALTER TABLE resync_co_alter_table_rename2 RENAME TO resync_co_alter_table_newname2;
ALTER TABLE sync2_co_alter_table_rename1 RENAME TO sync2_co_alter_table_newname1;

