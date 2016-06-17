
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema partitionindexes;
set search_path to partitionindexes;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: sql/alter_tab_add_pkey_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part6',False FROM generate_series(51,60)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description ALTER TABLE, Unique index with Primary Key
ALTER TABLE pt_lt_tab ADD primary key(col2,col1);
SELECT * FROM pt_lt_tab WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/alter_tab_add_pkey_6.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part6',False FROM generate_series(51,60)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description ALTER TABLE, Unique index with Primary Key
ALTER TABLE pt_lt_tab ADD primary key(col2,col1);
SELECT * FROM pt_lt_tab  WHERE col2 > 10 OR col1 = 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/alter_tab_add_uniquekey_3.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part6',False FROM generate_series(51,60)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description ALTER TABLE, Unique index with Primary Key, unique index on the default partition 
ALTER TABLE pt_lt_tab ADD unique(col2,col1);
SELECT * FROM pt_lt_tab WHERE col1 < 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/alter_tab_add_uniquekey_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part6',False FROM generate_series(51,60)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description ALTER TABLE, Unique index with Primary Key, unique index on the default partition 
ALTER TABLE pt_lt_tab ADD unique(col2,col1);
SELECT * FROM pt_lt_tab WHERE col1 > 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/alter_tab_add_uniquekey_5.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part6',False FROM generate_series(51,60)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description ALTER TABLE, Unique index with Primary Key, unique index on the default partition 
ALTER TABLE pt_lt_tab ADD unique(col2,col1);
SELECT * FROM pt_lt_tab WHERE col2 = 25 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/alter_tab_add_uniquekey_7.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part6',False FROM generate_series(51,60)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description ALTER TABLE, Unique index with Primary Key, unique index on the default partition 
ALTER TABLE pt_lt_tab ADD unique(col2,col1);
SELECT * FROM pt_lt_tab  WHERE col2 > 10 AND col1 = 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/alter_tab_add_uniquekey_9.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part6',False FROM generate_series(51,60)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description ALTER TABLE, Unique index with Primary Key, unique index on the default partition 
ALTER TABLE pt_lt_tab ADD unique(col2,col1);
SELECT * FROM pt_lt_tab  WHERE col2 between 10 AND 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/alter_tab_drop_col_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part6',False FROM generate_series(51,60)i;
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description ALTER TABLE, Drop column
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col4,col2);

ALTER TABLE pt_lt_tab DROP column col4;
SELECT * FROM pt_lt_tab WHERE col2 > 41 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_multikey_ptcol_df_1.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes 
-- @negtest True
-- @description Heterogeneous index,b-tree index on all parts,index , multiple index 
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2,col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2,col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2,col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2,col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2,col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2,col1);

SELECT * FROM pt_lt_tab_df WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_multikey_ptcol_df_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts,index , multiple index 
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2,col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2,col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2,col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2,col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2,col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2,col1);

SELECT * FROM pt_lt_tab_df WHERE col2 between 1 AND 100 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_multikey_ptcol_df_3.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts,index , multiple index 
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2,col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2,col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2,col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2,col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2,col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2,col1);

SELECT * FROM pt_lt_tab_df WHERE col2 < 50 AND col1 > 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_multikey_ptcol_df_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts,index , multiple index 
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2,col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2,col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2,col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2,col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2,col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2,col1);

SELECT * FROM pt_lt_tab_df WHERE col2 = 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_multikey_ptcol_df_5.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts,index , multiple index 
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2,col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2,col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2,col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2,col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2,col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2,col1);

SELECT * FROM pt_lt_tab_df WHERE col2 > 10 AND col1 between 1 AND 100 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_multikey_ptcol_df_6.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description Heterogeneous index,b-tree index on all parts,index , multiple index 
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2,col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2,col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2,col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2,col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2,col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2,col1);

SELECT * FROM pt_lt_tab_df WHERE col1 = 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_nonptcol_df_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts,index, index on non-partition col
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col1);

SELECT * FROM pt_lt_tab_df WHERE col1 between 1 AND 100 ORDER BY col1 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_nonptcol_df_3.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts,index, index on non-partition col
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col1);

SELECT * FROM pt_lt_tab_df WHERE col1 > 50 ORDER BY col1 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_nonptcol_df_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts,index, index on non-partition col
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col1);

SELECT * FROM pt_lt_tab_df WHERE col1 > 50 ORDER BY col1 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_nonptcol_df_5.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts,index, index on non-partition col
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col1);

SELECT * FROM pt_lt_tab_df WHERE col1 < 50 ORDER BY col1 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_ptcol_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index, index on partition key, b-tree index on all partitions
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_1_prt_part5(col2);
SELECT * FROM pt_lt_tab WHERE col2 between 1 AND 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_ptcol_3.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index, index on partition key, b-tree index on all partitions
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_1_prt_part5(col2);
SELECT * FROM pt_lt_tab WHERE col2 > 5 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_ptcol_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index, index on partition key, b-tree index on all partitions
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_1_prt_part5(col2);
SELECT * FROM pt_lt_tab WHERE col2 = 5 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_ptcol_df_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts including default, index on partition col
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2);
SELECT * FROM pt_lt_tab_df WHERE col2 between 1 AND 100 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_ptcol_df_3.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts including default, index on partition col
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2);
SELECT * FROM pt_lt_tab_df WHERE col2 > 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_ptcol_df_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts including default, index on partition col
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2);
SELECT * FROM pt_lt_tab_df WHERE col2 > 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/heterogeneous_singlekey_ptcol_df_5.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab_df;
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
	partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
	partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
	partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
	partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description Heterogeneous index,b-tree index on all parts including default, index on partition col
-- start_ignore
DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2);
SELECT * FROM pt_lt_tab_df WHERE col2 = 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_multikey_ptcol_1.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description multiple column b-tree index
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1,col2);
SELECT * FROM pt_lt_tab WHERE col1 < 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_multikey_ptcol_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description multiple column b-tree index
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1,col2);
SELECT * FROM pt_lt_tab WHERE col1 > 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_multikey_ptcol_3.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description multiple column b-tree index
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1,col2);
SELECT * FROM pt_lt_tab WHERE col2 = 25 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_multikey_ptcol_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description multiple column b-tree index
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1,col2);
SELECT * FROM pt_lt_tab WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_multikey_ptcol_5.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description multiple column b-tree index
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1,col2);
SELECT * FROM pt_lt_tab WHERE col2 > 10 AND col1 = 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_multikey_ptcol_6.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description multiple column b-tree index
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1,col2);
SELECT * FROM pt_lt_tab WHERE col2 > 10.00 OR col1 = 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_multikey_ptcol_7.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description multiple column b-tree index
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1,col2);
SELECT * FROM pt_lt_tab WHERE col2 between 10 AND 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_nonptcol_1.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description B-tree single index key = non-partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1);
SELECT * FROM pt_lt_tab WHERE col1 < 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_nonptcol_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description B-tree single index key = non-partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1);
SELECT * FROM pt_lt_tab WHERE col1 > 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_nonptcol_3.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description B-tree single index key = non-partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1);
SELECT * FROM pt_lt_tab WHERE col1 = 25 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_nonptcol_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description B-tree single index key = non-partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1);
SELECT * FROM pt_lt_tab WHERE col1 <> 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_nonptcol_5.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description B-tree single index key = non-partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1);
SELECT * FROM pt_lt_tab WHERE col1 > 10 AND col1 < 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_nonptcol_6.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description B-tree single index key = non-partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1);
SELECT * FROM pt_lt_tab WHERE col1 > 10 OR col1 = 25 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_nonptcol_7.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description B-tree single index key = non-partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col1);
SELECT * FROM pt_lt_tab WHERE col1 between 10 AND 25 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_ptcol_1.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description B-tree single index key = partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col2);
SELECT * FROM pt_lt_tab WHERE col2 < 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_ptcol_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description B-tree single index key = partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col2);
SELECT * FROM pt_lt_tab WHERE col2 > 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_ptcol_3.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description B-tree single index key = partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col2);
SELECT * FROM pt_lt_tab WHERE col2 = 25 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_ptcol_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description B-tree single index key = partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col2);
SELECT * FROM pt_lt_tab WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_ptcol_5.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description B-tree single index key = partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col2);
SELECT * FROM pt_lt_tab WHERE col2 > 10 AND col2 < 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_ptcol_6.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description B-tree single index key = partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col2);
SELECT * FROM pt_lt_tab WHERE col2 > 10 OR col2 = 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/homogeneous_singlekey_ptcol_7.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @description B-tree single index key = partitioning key
-- start_ignore
DROP INDEX IF EXISTS idx1;
-- end_ignore
CREATE INDEX idx1 on pt_lt_tab(col2);
SELECT * FROM pt_lt_tab WHERE col2 between 10 AND 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/neg_alter_tab_drop_idx_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'part6',False FROM generate_series(51,60)i;
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);

DROP INDEX IF EXISTS idx1;
CREATE INDEX idx1 on pt_lt_tab(col2);
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description Negative tests ALTER TABLE, drop index
-- start_ignore
DROP INDEX IF EXISTS idx1_1_prt_part4;
-- end_ignore
SELECT * FROM pt_lt_tab WHERE col2 = 35 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/neg_combination_consecutive_pt_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'def',False FROM generate_series(51,100)i;
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);

DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
CREATE INDEX idx1 on pt_lt_tab_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_1_prt_part3(col2);
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description Negative tests combination tests, Index exists on some continuous set of partitions, e.g. p1,p2,p3
SELECT * FROM pt_lt_tab WHERE col2 = 35 ORDER BY col2,col3 LIMIT 5;


-- ----------------------------------------------------------------------
-- Test: sql/neg_combination_no_idx_def_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'def',False FROM generate_series(51,100)i;
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);

DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;

CREATE INDEX idx1 on pt_lt_tab_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_1_prt_part5(col2);
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description Negative tests Combination tests , no index on default partition 
SELECT * FROM pt_lt_tab WHERE col2 > 51 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/neg_combination_no_idx_def_3.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'def',False FROM generate_series(51,100)i;
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);

DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx2;
DROP INDEX IF EXISTS idx3;
DROP INDEX IF EXISTS idx4;
DROP INDEX IF EXISTS idx5;

CREATE INDEX idx1 on pt_lt_tab_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_1_prt_part5(col2);
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description Negative tests Combination tests , no index on default partition 
SELECT * FROM pt_lt_tab WHERE col2 = 50 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: sql/neg_combination_no_idx_def_6.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'def',False FROM generate_series(51,100)i;
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);

DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx5;
CREATE INDEX idx1 on pt_lt_tab_1_prt_part1(col2);
CREATE INDEX idx5 on pt_lt_tab_1_prt_part5(col2);
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @description Negative tests Combination tests ,index exists on some regular partitions and not on the default partition
SELECT * FROM pt_lt_tab WHERE col2 is NULL ORDER BY col2,col3 LIMIT 5; 

-- ----------------------------------------------------------------------
-- Test: sql/neg_combination_non_consecutive_pt_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS pt_lt_tab;
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3  text,
  col4 bool
  
)
distributed by (col1)
partition by list(col2)
(
	partition part1 values(1,2,3,4,5,6,7,8,9,10),
	partition part2 values(11,12,13,14,15,16,17,18,19,20),
	partition part3 values(21,22,23,24,25,26,27,28,29,30),
	partition part4 values(31,32,33,34,35,36,37,38,39,40),
	partition part5 values(41,42,43,44,45,46,47,48,49,50),
        default partition def
);

INSERT INTO pt_lt_tab SELECT i, i,'part1',True FROM generate_series(1,10)i;
INSERT INTO pt_lt_tab SELECT i, i,'part2',True FROM generate_series(11,20)i;
INSERT INTO pt_lt_tab SELECT i, i,'part3',False FROM generate_series(21,30)i;
INSERT INTO pt_lt_tab SELECT i, i,'part4',False FROM generate_series(31,40)i;
INSERT INTO pt_lt_tab SELECT i, i,'part5',False FROM generate_series(41,50)i;
INSERT INTO pt_lt_tab SELECT i, i,'def',False FROM generate_series(51,100)i;
INSERT INTO pt_lt_tab VALUES(NULL,NULL,NULL,NULL);

DROP INDEX IF EXISTS idx1;
DROP INDEX IF EXISTS idx5;
DROP INDEX IF EXISTS idx6;
CREATE INDEX idx1 on pt_lt_tab_1_prt_part1(col2);
CREATE INDEX idx5 on pt_lt_tab_1_prt_part5(col2);
CREATE INDEX idx6 on pt_lt_tab_1_prt_def(col2);
ANALYZE pt_lt_tab;
-- end_ignore

-- @author prabhd
-- @modified 2013-08-01 12:00:00
-- @created 2013-08-01 12:00:00
-- @db_name ptidx
-- @tags partitionindexes
-- @negtest True
-- @ description Negative tests Combination tests, Index exists on some regular partitions and on the default partition [INDEX exists on non-consecutive partitions, e.g. p1,p3,p5]
SELECT * FROM pt_lt_tab WHERE col2 > 15 ORDER BY col2,col3 LIMIT 5;

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema partitionindexes cascade;
-- end_ignore
