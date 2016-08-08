-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

--
-- SYNC2
--
CREATE TABLE sync2_cluster_table_unique_index(
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

CREATE UNIQUE INDEX sync2_cluster_unq_idx1 ON sync2_cluster_table_unique_index (numeric_col);
CREATE UNIQUE INDEX sync2_cluster_unq_idx2 ON sync2_cluster_table_unique_index (numeric_col);


insert into sync2_cluster_table_unique_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into sync2_cluster_table_unique_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into sync2_cluster_table_unique_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into sync2_cluster_table_unique_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

select count(*) from sync2_cluster_table_unique_index;

CREATE TABLE sync2_cluster_table_btree_index(
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

CREATE INDEX sync2_cluster_btree_idx1 ON sync2_cluster_table_btree_index (numeric_col);
CREATE INDEX sync2_cluster_btree_idx2 ON sync2_cluster_table_btree_index (numeric_col);


insert into sync2_cluster_table_btree_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into sync2_cluster_table_btree_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into sync2_cluster_table_btree_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into sync2_cluster_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

select count(*) from sync2_cluster_table_btree_index;

CREATE TABLE sync2_cluster_table_gist_index (
 id INTEGER,
 property BOX, 
 filler VARCHAR DEFAULT 'This is here just to take up space so that we use more pages of data and sequential scans take a lot more time.  Stones tinheads and mixers coming; we did it all on our own; this summer I hear the crunching; 11 dead in Ohio. Got right down to it; we were cutting us down; could have had fun but, no; left them face down dead on the ground.  How can you listen when you know?'
 )
 DISTRIBUTED BY (id);


INSERT INTO sync2_cluster_table_gist_index (id, property) VALUES (1, '( (0,0), (1,1) )');
INSERT INTO sync2_cluster_table_gist_index (id, property) VALUES (2, '( (0,0), (2,2) )');
INSERT INTO sync2_cluster_table_gist_index (id, property) VALUES (3, '( (0,0), (3,3) )');
INSERT INTO sync2_cluster_table_gist_index (id, property) VALUES (4, '( (0,0), (4,4) )');
INSERT INTO sync2_cluster_table_gist_index (id, property) VALUES (5, '( (0,0), (5,5) )');

select count(*) from sync2_cluster_table_gist_index;

CREATE INDEX sync2_cluster_gist_idx1 ON sync2_cluster_table_gist_index USING GiST (property);
CREATE INDEX sync2_cluster_gist_idx2 ON sync2_cluster_table_gist_index USING GiST (property);


select count(*) from sync2_cluster_table_gist_index;


--
--  CLUSTER     INDEX
--
--
-- UNIQUE INDEX
--
 CLUSTER  sync1_cluster_unq_idx7  ON   sync1_cluster_table_unique_index  ;
insert into sync1_cluster_table_unique_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1301,1400)i;
select count(*) from sync1_cluster_table_unique_index;
--
--
 CLUSTER  ck_sync1_cluster_unq_idx6   ON   ck_sync1_cluster_table_unique_index  ;
insert into ck_sync1_cluster_table_unique_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1301,1400)i;
select count(*) from ck_sync1_cluster_table_unique_index;
--
--
 CLUSTER  ct_cluster_unq_idx4  ON   ct_cluster_table_unique_index  ;
insert into ct_cluster_table_unique_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1301,1400)i;
select count(*) from ct_cluster_table_unique_index;
--
--
 CLUSTER  resync_cluster_unq_idx2  ON   resync_cluster_table_unique_index  ;
insert into resync_cluster_table_unique_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1301,1400)i;
select count(*) from resync_cluster_table_unique_index;
--
--
 CLUSTER  sync2_cluster_unq_idx1  ON   sync2_cluster_table_unique_index  ;
insert into sync2_cluster_table_unique_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(1301,1400)i;
select count(*) from sync2_cluster_table_unique_index;

--
-- BTREE INDEX
--

 CLUSTER  sync1_cluster_btree_idx7  ON   sync1_cluster_table_btree_index  ;
insert into sync1_cluster_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(101,200)i;
select count(*) from sync1_cluster_table_btree_index;
--
--
 CLUSTER  ck_sync1_cluster_btree_idx6  ON   ck_sync1_cluster_table_btree_index  ;
insert into ck_sync1_cluster_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(101,200)i;
select count(*) from ck_sync1_cluster_table_btree_index;
--
--
 CLUSTER  ct_cluster_btree_idx4  ON   ct_cluster_table_btree_index  ;
insert into ct_cluster_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(101,200)i;
select count(*) from ct_cluster_table_btree_index;
--
--
 CLUSTER  resync_cluster_btree_idx2  ON   resync_cluster_table_btree_index  ;
insert into resync_cluster_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(101,200)i;
select count(*) from resync_cluster_table_btree_index;
--
--
 CLUSTER  sync2_cluster_btree_idx1  ON   sync2_cluster_table_btree_index  ;
insert into sync2_cluster_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(101,200)i;
select count(*) from sync2_cluster_table_btree_index;

--
-- GIST INDEX
--
 CLUSTER  sync1_cluster_gist_idx7  ON   sync1_cluster_table_gist_index  ;
INSERT INTO sync1_cluster_table_gist_index (id, property) VALUES (6, '( (0,0), (6,6) )');
select count(*) from sync1_cluster_table_gist_index;
--
--
 CLUSTER ck_sync1_cluster_gist_idx6   ON   ck_sync1_cluster_table_gist_index  ;
INSERT INTO ck_sync1_cluster_table_gist_index (id, property) VALUES (6, '( (0,0), (6,6) )');
select count(*) from ck_sync1_cluster_table_gist_index;
--
--
 CLUSTER   ct_cluster_gist_idx4  ON   ct_cluster_table_gist_index  ;
INSERT INTO ct_cluster_table_gist_index (id, property) VALUES (6, '( (0,0), (6,6) )');
select count(*) from ct_cluster_table_gist_index;
--
--
 CLUSTER  resync_cluster_gist_idx2  ON   resync_cluster_table_gist_index  ;
INSERT INTO resync_cluster_table_gist_index (id, property) VALUES (6, '( (0,0), (6,6) )');
select count(*) from resync_cluster_table_gist_index;
--
--
 CLUSTER   sync2_cluster_gist_idx1 ON   sync2_cluster_table_gist_index  ;
INSERT INTO sync2_cluster_table_gist_index (id, property) VALUES (6, '( (0,0), (6,6) )');
select count(*) from sync2_cluster_table_gist_index;


