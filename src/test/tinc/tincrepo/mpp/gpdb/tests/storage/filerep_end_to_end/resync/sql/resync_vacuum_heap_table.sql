-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

CREATE TABLE resync_heap_vacuum1(
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

INSERT INTO resync_heap_vacuum1 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO resync_heap_vacuum1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO resync_heap_vacuum1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO resync_heap_vacuum1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



ALTER TABLE resync_heap_vacuum1 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE resync_heap_vacuum1 DROP COLUMN drop_col ;
ALTER TABLE resync_heap_vacuum1 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE resync_heap_vacuum1 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE resync_heap_vacuum1 set with ( reorganize='true') distributed by (int_col);


DELETE FROM  resync_heap_vacuum1 WHERE text_col='0_zero';
DELETE FROM  resync_heap_vacuum1 WHERE text_col='1_zero';
DELETE FROM  resync_heap_vacuum1 WHERE text_col='2_zero';


INSERT INTO resync_heap_vacuum1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO resync_heap_vacuum1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO resync_heap_vacuum1 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


UPDATE resync_heap_vacuum1 SET text_col='1_one' where text_col='1_zero';
UPDATE resync_heap_vacuum1 SET text_col='2_two' where text_col='2_zero';
UPDATE resync_heap_vacuum1 SET text_col='3_three' where text_col='3_zero';



CREATE TABLE resync_heap_vacuum2(
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

INSERT INTO resync_heap_vacuum2 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO resync_heap_vacuum2 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO resync_heap_vacuum2 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO resync_heap_vacuum2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



ALTER TABLE resync_heap_vacuum2 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE resync_heap_vacuum2 DROP COLUMN drop_col ;
ALTER TABLE resync_heap_vacuum2 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE resync_heap_vacuum2 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE resync_heap_vacuum2 set with ( reorganize='true') distributed by (int_col);


DELETE FROM  resync_heap_vacuum2 WHERE text_col='0_zero';
DELETE FROM  resync_heap_vacuum2 WHERE text_col='1_zero';
DELETE FROM  resync_heap_vacuum2 WHERE text_col='2_zero';


INSERT INTO resync_heap_vacuum2 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO resync_heap_vacuum2 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO resync_heap_vacuum2 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


UPDATE resync_heap_vacuum2 SET text_col='1_one' where text_col='1_zero';
UPDATE resync_heap_vacuum2 SET text_col='2_two' where text_col='2_zero';
UPDATE resync_heap_vacuum2 SET text_col='3_three' where text_col='3_zero';



CREATE TABLE resync_heap_vacuum3(
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

INSERT INTO resync_heap_vacuum3 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO resync_heap_vacuum3 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO resync_heap_vacuum3 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO resync_heap_vacuum3 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



ALTER TABLE resync_heap_vacuum3 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE resync_heap_vacuum3 DROP COLUMN drop_col ;
ALTER TABLE resync_heap_vacuum3 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE resync_heap_vacuum3 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE resync_heap_vacuum3 set with ( reorganize='true') distributed by (int_col);


DELETE FROM  resync_heap_vacuum3 WHERE text_col='0_zero';
DELETE FROM  resync_heap_vacuum3 WHERE text_col='1_zero';
DELETE FROM  resync_heap_vacuum3 WHERE text_col='2_zero';


INSERT INTO resync_heap_vacuum3 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO resync_heap_vacuum3 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO resync_heap_vacuum3 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


UPDATE resync_heap_vacuum3 SET text_col='1_one' where text_col='1_zero';
UPDATE resync_heap_vacuum3 SET text_col='2_two' where text_col='2_zero';
UPDATE resync_heap_vacuum3 SET text_col='3_three' where text_col='3_zero';


VACUUM sync1_heap_vacuum6;
VACUUM ck_sync1_heap_vacuum5;
VACUUM ct_heap_vacuum3;
VACUUM resync_heap_vacuum1;


