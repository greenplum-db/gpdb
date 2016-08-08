-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

CREATE TABLE ct_heap_analyze1(
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

INSERT INTO ct_heap_analyze1 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ct_heap_analyze1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ct_heap_analyze1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ct_heap_analyze1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



ALTER TABLE ct_heap_analyze1 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ct_heap_analyze1 DROP COLUMN drop_col ;
ALTER TABLE ct_heap_analyze1 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ct_heap_analyze1 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ct_heap_analyze1 set with ( reorganize='true') distributed by (int_col);


DELETE FROM  ct_heap_analyze1 WHERE text_col='0_zero';
DELETE FROM  ct_heap_analyze1 WHERE text_col='1_zero';
DELETE FROM  ct_heap_analyze1 WHERE text_col='2_zero';


INSERT INTO ct_heap_analyze1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ct_heap_analyze1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ct_heap_analyze1 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


UPDATE ct_heap_analyze1 SET text_col='1_one' where text_col='1_zero';
UPDATE ct_heap_analyze1 SET text_col='2_two' where text_col='2_zero';
UPDATE ct_heap_analyze1 SET text_col='3_three' where text_col='3_zero';



CREATE TABLE ct_heap_analyze2(
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

INSERT INTO ct_heap_analyze2 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ct_heap_analyze2 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ct_heap_analyze2 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ct_heap_analyze2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



ALTER TABLE ct_heap_analyze2 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ct_heap_analyze2 DROP COLUMN drop_col ;
ALTER TABLE ct_heap_analyze2 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ct_heap_analyze2 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ct_heap_analyze2 set with ( reorganize='true') distributed by (int_col);


DELETE FROM  ct_heap_analyze2 WHERE text_col='0_zero';
DELETE FROM  ct_heap_analyze2 WHERE text_col='1_zero';
DELETE FROM  ct_heap_analyze2 WHERE text_col='2_zero';


INSERT INTO ct_heap_analyze2 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ct_heap_analyze2 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ct_heap_analyze2 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


UPDATE ct_heap_analyze2 SET text_col='1_one' where text_col='1_zero';
UPDATE ct_heap_analyze2 SET text_col='2_two' where text_col='2_zero';
UPDATE ct_heap_analyze2 SET text_col='3_three' where text_col='3_zero';



CREATE TABLE ct_heap_analyze3(
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

INSERT INTO ct_heap_analyze3 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ct_heap_analyze3 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ct_heap_analyze3 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ct_heap_analyze3 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



ALTER TABLE ct_heap_analyze3 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ct_heap_analyze3 DROP COLUMN drop_col ;
ALTER TABLE ct_heap_analyze3 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ct_heap_analyze3 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ct_heap_analyze3 set with ( reorganize='true') distributed by (int_col);


DELETE FROM  ct_heap_analyze3 WHERE text_col='0_zero';
DELETE FROM  ct_heap_analyze3 WHERE text_col='1_zero';
DELETE FROM  ct_heap_analyze3 WHERE text_col='2_zero';


INSERT INTO ct_heap_analyze3 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ct_heap_analyze3 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ct_heap_analyze3 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


UPDATE ct_heap_analyze3 SET text_col='1_one' where text_col='1_zero';
UPDATE ct_heap_analyze3 SET text_col='2_two' where text_col='2_zero';
UPDATE ct_heap_analyze3 SET text_col='3_three' where text_col='3_zero';


CREATE TABLE ct_heap_analyze4(
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

INSERT INTO ct_heap_analyze4 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ct_heap_analyze4 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ct_heap_analyze4 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ct_heap_analyze4 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



ALTER TABLE ct_heap_analyze4 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ct_heap_analyze4 DROP COLUMN drop_col ;
ALTER TABLE ct_heap_analyze4 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ct_heap_analyze4 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ct_heap_analyze4 set with ( reorganize='true') distributed by (int_col);


DELETE FROM  ct_heap_analyze4 WHERE text_col='0_zero';
DELETE FROM  ct_heap_analyze4 WHERE text_col='1_zero';
DELETE FROM  ct_heap_analyze4 WHERE text_col='2_zero';


INSERT INTO ct_heap_analyze4 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ct_heap_analyze4 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ct_heap_analyze4 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


UPDATE ct_heap_analyze4 SET text_col='1_one' where text_col='1_zero';
UPDATE ct_heap_analyze4 SET text_col='2_two' where text_col='2_zero';
UPDATE ct_heap_analyze4 SET text_col='3_three' where text_col='3_zero';


CREATE TABLE ct_heap_analyze5(
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

INSERT INTO ct_heap_analyze5 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
INSERT INTO ct_heap_analyze5 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ct_heap_analyze5 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ct_heap_analyze5 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;



ALTER TABLE ct_heap_analyze5 ADD COLUMN added_col character varying(30) default 'test_value';
ALTER TABLE ct_heap_analyze5 DROP COLUMN drop_col ;
ALTER TABLE ct_heap_analyze5 RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE ct_heap_analyze5 ALTER COLUMN change_datatype_col TYPE int4;
ALTER TABLE ct_heap_analyze5 set with ( reorganize='true') distributed by (int_col);


DELETE FROM  ct_heap_analyze5 WHERE text_col='0_zero';
DELETE FROM  ct_heap_analyze5 WHERE text_col='1_zero';
DELETE FROM  ct_heap_analyze5 WHERE text_col='2_zero';


INSERT INTO ct_heap_analyze5 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}',  1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
INSERT INTO ct_heap_analyze5 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}',  2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
INSERT INTO ct_heap_analyze5 values ('3_zero', 3, '3_zero', 0, 0, 0, '{0}',  0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');


UPDATE ct_heap_analyze5 SET text_col='1_one' where text_col='1_zero';
UPDATE ct_heap_analyze5 SET text_col='2_two' where text_col='2_zero';
UPDATE ct_heap_analyze5 SET text_col='3_three' where text_col='3_zero';



ANALYZE sync1_heap_analyze4;
ANALYZE ck_sync1_heap_analyze3;
ANALYZE ct_heap_analyze1;


