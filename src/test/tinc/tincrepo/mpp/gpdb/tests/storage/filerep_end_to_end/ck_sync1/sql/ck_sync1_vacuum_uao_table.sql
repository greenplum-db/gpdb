-- @product_version gpdb: [4.3.0.0-]
CREATE TABLE ck_sync1_uao_vacuum1(
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
date_column date)with (appendonly=true) distributed randomly;

INSERT INTO ck_sync1_uao_vacuum1 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ck_sync1_uao_vacuum1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;


select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='ck_sync1_uao_vacuum1');
select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum1;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum1 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum1;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum1  where bigint_col >=  50;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum1;
set gp_select_invisible = false;

ALTER TABLE ck_sync1_uao_vacuum1 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ck_sync1_uao_vacuum1 DROP COLUMN drop_col ;
ALTER TABLE ck_sync1_uao_vacuum1 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ck_sync1_uao_vacuum1 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ck_sync1_uao_vacuum1 set with ( reorganize='true') distributed by (int_col);


INSERT INTO ck_sync1_uao_vacuum1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum1 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');

select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum1;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum1 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum1;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum1  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum1;
set gp_select_invisible = false;

CREATE TABLE ck_sync1_uao_vacuum2(
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
date_column date) with (appendonly=true) distributed randomly;


INSERT INTO ck_sync1_uao_vacuum2 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ck_sync1_uao_vacuum2 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum2 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='ck_sync1_uao_vacuum2');
select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum2;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum2;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum2 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum2;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum2;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum2  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum2;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum2;
set gp_select_invisible = false;

ALTER TABLE ck_sync1_uao_vacuum2 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ck_sync1_uao_vacuum2 DROP COLUMN drop_col ;
ALTER TABLE ck_sync1_uao_vacuum2 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ck_sync1_uao_vacuum2 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ck_sync1_uao_vacuum2 set with ( reorganize='true') distributed by (int_col);


INSERT INTO ck_sync1_uao_vacuum2 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum2 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum2 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum2;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum2;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum2 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum2;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum2;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum2  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum2;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum2;
set gp_select_invisible = false;

CREATE TABLE ck_sync1_uao_vacuum3(
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
date_column date) with (appendonly=true) distributed randomly;

INSERT INTO ck_sync1_uao_vacuum3 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ck_sync1_uao_vacuum3 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum3 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum3 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;


select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='ck_sync1_uao_vacuum3');
select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum3;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum3;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum3 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum3;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum3;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum3  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum3;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum3;
set gp_select_invisible = false;

ALTER TABLE ck_sync1_uao_vacuum3 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ck_sync1_uao_vacuum3 DROP COLUMN drop_col ;
ALTER TABLE ck_sync1_uao_vacuum3 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ck_sync1_uao_vacuum3 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ck_sync1_uao_vacuum3 set with ( reorganize='true') distributed by (int_col);


INSERT INTO ck_sync1_uao_vacuum3 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum3 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum3 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum3;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum3;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum3 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum3;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum3;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum3  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum3;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum3;
set gp_select_invisible = false;

CREATE TABLE ck_sync1_uao_vacuum4(
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
date_column date) with (appendonly=true) distributed randomly;

INSERT INTO ck_sync1_uao_vacuum4 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ck_sync1_uao_vacuum4 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum4 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum4 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='ck_sync1_uao_vacuum4');
select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum4;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum4;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum4 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum4;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum4;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum4  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum4;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum4;
set gp_select_invisible = false;


ALTER TABLE ck_sync1_uao_vacuum4 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ck_sync1_uao_vacuum4 DROP COLUMN drop_col ;
ALTER TABLE ck_sync1_uao_vacuum4 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ck_sync1_uao_vacuum4 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ck_sync1_uao_vacuum4 set with ( reorganize='true') distributed by (int_col);


INSERT INTO ck_sync1_uao_vacuum4 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum4 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum4 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');

select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum4;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum4;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum4 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum4;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum4;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum4  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum4;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum4;
set gp_select_invisible = false;

CREATE TABLE ck_sync1_uao_vacuum5(
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
date_column date)with (appendonly=true) distributed randomly;

INSERT INTO ck_sync1_uao_vacuum5 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ck_sync1_uao_vacuum5 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum5 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum5 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;


select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='ck_sync1_uao_vacuum5');
select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum5;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum5;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum5 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum5;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum5;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum5  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum5;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum5;
set gp_select_invisible = false;

ALTER TABLE ck_sync1_uao_vacuum5 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ck_sync1_uao_vacuum5 DROP COLUMN drop_col ;
ALTER TABLE ck_sync1_uao_vacuum5 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ck_sync1_uao_vacuum5 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ck_sync1_uao_vacuum5 set with ( reorganize='true') distributed by (int_col);


INSERT INTO ck_sync1_uao_vacuum5 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum5 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum5 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum5;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum5;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum5 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum5;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum5;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum5  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum5;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum5;
set gp_select_invisible = false;

CREATE TABLE ck_sync1_uao_vacuum6(
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
date_column date)with (appendonly=true) distributed randomly;

INSERT INTO ck_sync1_uao_vacuum6 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ck_sync1_uao_vacuum6 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum6 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum6 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;


select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='ck_sync1_uao_vacuum6');
select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum6;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum6;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum6 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum6;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum6;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum6  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum6;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum6;
set gp_select_invisible = false;


ALTER TABLE ck_sync1_uao_vacuum6 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ck_sync1_uao_vacuum6 DROP COLUMN drop_col ;
ALTER TABLE ck_sync1_uao_vacuum6 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ck_sync1_uao_vacuum6 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ck_sync1_uao_vacuum6 set with ( reorganize='true') distributed by (int_col);


INSERT INTO ck_sync1_uao_vacuum6 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum6 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum6 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');

select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum6;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum6;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum6 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum6;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum6;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum6  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum6;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum6;
set gp_select_invisible = false;

CREATE TABLE ck_sync1_uao_vacuum7(
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
date_column date)with (appendonly=true) distributed randomly;

INSERT INTO ck_sync1_uao_vacuum7 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ck_sync1_uao_vacuum7 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum7 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum7 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;


select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='ck_sync1_uao_vacuum7');
select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum7;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum7;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum7 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum7;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum7;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum7  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum7;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum7;
set gp_select_invisible = false;


ALTER TABLE ck_sync1_uao_vacuum7 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ck_sync1_uao_vacuum7 DROP COLUMN drop_col ;
ALTER TABLE ck_sync1_uao_vacuum7 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ck_sync1_uao_vacuum7 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ck_sync1_uao_vacuum7 set with ( reorganize='true') distributed by (int_col);


INSERT INTO ck_sync1_uao_vacuum7 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ck_sync1_uao_vacuum7 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ck_sync1_uao_vacuum7 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


select count(*) AS only_visi_tups_ins  from ck_sync1_uao_vacuum7;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_vacuum7;
set gp_select_invisible = false;
update ck_sync1_uao_vacuum7 set text_col = text_col || '_new' where bigint_col = 1;
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_vacuum7;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum7;
set gp_select_invisible = false;
delete from ck_sync1_uao_vacuum7  where bigint_col =  3;
select count(*) AS only_visi_tups  from ck_sync1_uao_vacuum7;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_vacuum7;
set gp_select_invisible = false;
VACUUM sync1_uao_vacuum2;
VACUUM ck_sync1_uao_vacuum1;


