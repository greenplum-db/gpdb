-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE resync_heap_table_unique_index(
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
date_column date) ;

CREATE UNIQUE INDEX resync_heap_unq_idx1 ON resync_heap_table_unique_index (numeric_col);
CREATE UNIQUE INDEX resync_heap_unq_idx2 ON resync_heap_table_unique_index (numeric_col);
CREATE UNIQUE INDEX resync_heap_unq_idx3 ON resync_heap_table_unique_index (numeric_col);

insert into resync_heap_table_unique_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into resync_heap_table_unique_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into resync_heap_table_unique_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into resync_heap_table_unique_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE resync_heap_table_btree_index(
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
date_column date) ;

CREATE INDEX resync_heap_btree_idx1 ON resync_heap_table_btree_index (numeric_col);
CREATE INDEX resync_heap_btree_idx2 ON resync_heap_table_btree_index (numeric_col);
CREATE INDEX resync_heap_btree_idx3 ON resync_heap_table_btree_index (numeric_col);

insert into resync_heap_table_btree_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into resync_heap_table_btree_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into resync_heap_table_btree_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into resync_heap_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE resync_heap_table_bitmap_index(
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
date_column date) ;

CREATE INDEX resync_heap_bm_idx1 ON resync_heap_table_bitmap_index USING bitmap (numeric_col);;
CREATE INDEX resync_heap_bm_idx2 ON resync_heap_table_bitmap_index USING bitmap (numeric_col);;
CREATE INDEX resync_heap_bm_idx3 ON resync_heap_table_bitmap_index USING bitmap (numeric_col);;

insert into resync_heap_table_bitmap_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into resync_heap_table_bitmap_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into resync_heap_table_bitmap_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into resync_heap_table_bitmap_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE resync_heap_table_gist_index (
 id INTEGER,
 property BOX,
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers heapming; we did it all on our own; this summer I hear the crunching; 11
 dead in Ohio. Got right down to it; we were cutting us down; heapuld have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 DISTRIBUTED BY (id);


INSERT INTO resync_heap_table_gist_index (id, property) VALUES (1, '( (0,0), (1,1) )');
INSERT INTO resync_heap_table_gist_index (id, property) VALUES (2, '( (0,0), (2,2) )');
INSERT INTO resync_heap_table_gist_index (id, property) VALUES (3, '( (0,0), (3,3) )');
INSERT INTO resync_heap_table_gist_index (id, property) VALUES (4, '( (0,0), (4,4) )');
INSERT INTO resync_heap_table_gist_index (id, property) VALUES (5, '( (0,0), (5,5) )');

CREATE INDEX resync_heap_gist_idx1 ON resync_heap_table_gist_index USING GiST (property);
CREATE INDEX resync_heap_gist_idx2 ON resync_heap_table_gist_index USING GiST (property);
CREATE INDEX resync_heap_gist_idx3 ON resync_heap_table_gist_index USING GiST (property);

DROP INDEX sync1_heap_unq_idx6;
DROP INDEX sync1_heap_btree_idx6;
DROP INDEX sync1_heap_bm_idx6;
DROP INDEX sync1_heap_gist_idx6;
DROP INDEX ck_sync1_heap_unq_idx5;
DROP INDEX ck_sync1_heap_btree_idx5;
DROP INDEX ck_sync1_heap_bm_idx5;
DROP INDEX ck_sync1_heap_gist_idx5;
DROP INDEX ct_heap_unq_idx3;
DROP INDEX ct_heap_btree_idx3;
DROP INDEX ct_heap_bm_idx3;
DROP INDEX ct_heap_gist_idx3;
DROP INDEX resync_heap_unq_idx1;
DROP INDEX resync_heap_btree_idx1;
DROP INDEX resync_heap_bm_idx1;
DROP INDEX resync_heap_gist_idx1;

-- AO Index

CREATE TABLE resync_ao_table_btree_index(
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
date_column date) with (appendonly=true) ;

CREATE INDEX resync_ao_btree_idx1 ON resync_ao_table_btree_index (numeric_col);
CREATE INDEX resync_ao_btree_idx2 ON resync_ao_table_btree_index (numeric_col);
CREATE INDEX resync_ao_btree_idx3 ON resync_ao_table_btree_index (numeric_col);


insert into resync_ao_table_btree_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into resync_ao_table_btree_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into resync_ao_table_btree_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into resync_ao_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE resync_ao_table_bitmap_index(
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
date_column date) with (appendonly=true) ;

CREATE INDEX resync_ao_bm_idx1 ON resync_ao_table_bitmap_index USING bitmap (numeric_col);;
CREATE INDEX resync_ao_bm_idx2 ON resync_ao_table_bitmap_index USING bitmap (numeric_col);;
CREATE INDEX resync_ao_bm_idx3 ON resync_ao_table_bitmap_index USING bitmap (numeric_col);;

insert into resync_ao_table_bitmap_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into resync_ao_table_bitmap_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into resync_ao_table_bitmap_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into resync_ao_table_bitmap_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;


CREATE TABLE resync_ao_table_gist_index (
 id INTEGER,
 property BOX,
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11
 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 DISTRIBUTED BY (id);


INSERT INTO resync_ao_table_gist_index (id, property) VALUES (1, '( (0,0), (1,1) )');
INSERT INTO resync_ao_table_gist_index (id, property) VALUES (2, '( (0,0), (2,2) )');
INSERT INTO resync_ao_table_gist_index (id, property) VALUES (3, '( (0,0), (3,3) )');
INSERT INTO resync_ao_table_gist_index (id, property) VALUES (4, '( (0,0), (4,4) )');
INSERT INTO resync_ao_table_gist_index (id, property) VALUES (5, '( (0,0), (5,5) )');

CREATE INDEX resync_ao_gist_idx1 ON resync_ao_table_gist_index USING GiST (property);
CREATE INDEX resync_ao_gist_idx2 ON resync_ao_table_gist_index USING GiST (property);
CREATE INDEX resync_ao_gist_idx3 ON resync_ao_table_gist_index USING GiST (property);




DROP INDEX sync1_ao_btree_idx6;
DROP INDEX sync1_ao_bm_idx6;
DROP INDEX sync1_ao_gist_idx6;
DROP INDEX ck_sync1_ao_btree_idx5;
DROP INDEX ck_sync1_ao_bm_idx5;
DROP INDEX ck_sync1_ao_gist_idx5;
DROP INDEX ct_ao_btree_idx3;
DROP INDEX ct_ao_bm_idx3;
DROP INDEX ct_ao_gist_idx3;
DROP INDEX resync_ao_btree_idx1;
DROP INDEX resync_ao_bm_idx1;
DROP INDEX resync_ao_gist_idx1;

-- CO Index

CREATE TABLE resync_co_table_btree_index(
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
date_column date) with (orientation='column',appendonly=true) ;

CREATE INDEX resync_co_btree_idx1 ON resync_co_table_btree_index (numeric_col);
CREATE INDEX resync_co_btree_idx2 ON resync_co_table_btree_index (numeric_col);
CREATE INDEX resync_co_btree_idx3 ON resync_co_table_btree_index (numeric_col);


insert into resync_co_table_btree_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into resync_co_table_btree_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into resync_co_table_btree_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into resync_co_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE resync_co_table_bitmap_index(
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
date_column date) with (orientation='column',appendonly=true);

CREATE INDEX resync_co_bm_idx1 ON resync_co_table_bitmap_index USING bitmap (numeric_col);;
CREATE INDEX resync_co_bm_idx2 ON resync_co_table_bitmap_index USING bitmap (numeric_col);;
CREATE INDEX resync_co_bm_idx3 ON resync_co_table_bitmap_index USING bitmap (numeric_col);;

insert into resync_co_table_bitmap_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into resync_co_table_bitmap_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into resync_co_table_bitmap_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into resync_co_table_bitmap_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

CREATE TABLE resync_co_table_gist_index (
 id INTEGER,
 property BOX,
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11
 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 DISTRIBUTED BY (id);


INSERT INTO resync_co_table_gist_index (id, property) VALUES (1, '( (0,0), (1,1) )');
INSERT INTO resync_co_table_gist_index (id, property) VALUES (2, '( (0,0), (2,2) )');
INSERT INTO resync_co_table_gist_index (id, property) VALUES (3, '( (0,0), (3,3) )');
INSERT INTO resync_co_table_gist_index (id, property) VALUES (4, '( (0,0), (4,4) )');
INSERT INTO resync_co_table_gist_index (id, property) VALUES (5, '( (0,0), (5,5) )');

CREATE INDEX resync_co_gist_idx1 ON resync_co_table_gist_index USING GiST (property);
CREATE INDEX resync_co_gist_idx2 ON resync_co_table_gist_index USING GiST (property);
CREATE INDEX resync_co_gist_idx3 ON resync_co_table_gist_index USING GiST (property);

DROP INDEX sync1_co_btree_idx6;
DROP INDEX sync1_co_bm_idx6;
DROP INDEX sync1_co_gist_idx6;
DROP INDEX ck_sync1_co_btree_idx5;
DROP INDEX ck_sync1_co_bm_idx5;
DROP INDEX ck_sync1_co_gist_idx5;
DROP INDEX ct_co_btree_idx3;
DROP INDEX ct_co_bm_idx3;
DROP INDEX ct_co_gist_idx3;
DROP INDEX resync_co_btree_idx1;
DROP INDEX resync_co_bm_idx1;
DROP INDEX resync_co_gist_idx1;

