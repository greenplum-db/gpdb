-- MPP-20466 Dis-allow duplicate constraint names for same table
create table dupconstr (
						i int,
						j int constraint dup_constraint CHECK (j > 10))
						distributed by (i);
-- should fail because of duplicate constraint name
alter table dupconstr add constraint dup_constraint unique (i);
alter table dupconstr add constraint dup_constraint primary key (i);
-- cleanup
drop table dupconstr;

--
-- Alter datatype of column with constraint should raise meaningful error
-- See github issue: https://github.com/greenplum-db/gpdb/issues/10561
--
create table contype (i int4 primary key, j int check (j < 100));
alter table contype alter i type numeric; --error

insert into contype values (1, 1), (2, 2), (3, 3);
-- after insert data, alter primary key/unique column's type will go through a special check logic
alter table contype alter i type numeric; --error

alter table contype alter j type numeric;
-- cleanup
drop table contype;

--
-- Test ALTER COLUMN TYPE after dropped column with text datatype (see MPP-19146)
--
create domain mytype as text;
create temp table at_foo (f1 text, f2 mytype, f3 text);
insert into at_foo values('aa','bb','cc');
drop domain mytype cascade;
alter table at_foo alter f1 TYPE varchar(10);

drop table at_foo;
create domain mytype as int;
create temp table at_foo (f1 text, f2 mytype, f3 text);
insert into at_foo values('aa',0,'cc');
drop domain mytype cascade;
alter table at_foo alter f1 TYPE varchar(10);


-- Verify that INSERT, UPDATE and DELETE work after dropping a column and
-- adding a constraint. There was a bug on that in ORCA, once upon a time
-- (MPP-20207)

CREATE TABLE altable(a int, b text, c int);
ALTER TABLE altable DROP COLUMN b;

ALTER TABLE altable ADD CONSTRAINT c_check CHECK (c > 0);
INSERT INTO altable(a, c) VALUES(0, -10);
SELECT * FROM altable ORDER BY 1;
INSERT INTO altable(a, c) VALUES(0, 10);
SELECT * FROM altable ORDER BY 1;

DELETE FROM altable WHERE c = -10;
SELECT * FROM altable ORDER BY 1;
DELETE FROM altable WHERE c = 10;
SELECT * FROM altable ORDER BY 1;
DELETE FROM altable WHERE c = 10;
SELECT * FROM altable ORDER BY 1;

INSERT INTO altable(a, c) VALUES(0, 10);
SELECT * FROM altable ORDER BY 1;
UPDATE altable SET c = -10;
SELECT * FROM altable ORDER BY 1;
UPDATE altable SET c = 1;
SELECT * FROM altable ORDER BY 1;


-- Verify that changing the datatype of a funnily-named column works.
-- (There used to be a quoting bug in the internal query this issues.)
create table "foo'bar" (id int4, t text);
alter table "foo'bar" alter column t type integer using length(t);

--
-- ADD/DROP/ALTER COLUMN on root partition is approved.
--
-- Heap table
DROP TABLE IF EXISTS test_part_col;
CREATE TABLE test_part_col(a int, b int, c int, d int, e int)
DISTRIBUTED BY(a)
PARTITION BY RANGE (b)
( START (1) END (2) EVERY (1),
    DEFAULT PARTITION other_b);

ALTER TABLE ONLY test_part_col ADD COLUMN f int; --error
ALTER TABLE test_part_col ADD COLUMN f int;

ALTER TABLE ONLY test_part_col ALTER COLUMN f TYPE TEXT; --error
ALTER TABLE test_part_col ALTER COLUMN f TYPE TEXT;

ALTER TABLE ONLY test_part_col DROP COLUMN f; --error
ALTER TABLE test_part_col DROP COLUMN f;

ALTER TABLE ONLY test_part_col_1_prt_other_b ADD COLUMN f int;
ALTER TABLE test_part_col_1_prt_other_b ADD COLUMN f int;

ALTER TABLE ONLY test_part_col_1_prt_other_b ALTER COLUMN e TYPE TEXT; --error
ALTER TABLE test_part_col_1_prt_other_b ALTER COLUMN e TYPE TEXT;

ALTER TABLE ONLY test_part_col_1_prt_other_b DROP COLUMN e; --error
ALTER TABLE test_part_col_1_prt_other_b DROP COLUMN e;

DROP TABLE test_part_col;

-- Non-partition heap table
CREATE TABLE test_part_col(a int, b int, c int, d int, e int) DISTRIBUTED BY(a);

ALTER TABLE ONLY test_part_col ADD COLUMN f int;
ALTER TABLE ONLY test_part_col ALTER COLUMN f TYPE TEXT;
ALTER TABLE ONLY test_part_col DROP COLUMN f;

ALTER TABLE test_part_col ADD COLUMN f int;
ALTER TABLE test_part_col ALTER COLUMN f TYPE TEXT;
ALTER TABLE test_part_col DROP COLUMN f;

DROP TABLE test_part_col;

-- AO table
CREATE TABLE test_part_col(a int, b int, c int, d int, e int)
WITH (appendonly=true)
DISTRIBUTED BY(a)
PARTITION BY RANGE (b)
( START (1) END (2) EVERY (1),
    DEFAULT PARTITION other_b);

ALTER TABLE ONLY test_part_col ADD COLUMN f int; --error
ALTER TABLE test_part_col ADD COLUMN f int;

ALTER TABLE ONLY test_part_col ALTER COLUMN f TYPE TEXT; --error
ALTER TABLE test_part_col ALTER COLUMN f TYPE TEXT;

ALTER TABLE ONLY test_part_col DROP COLUMN f; --error
ALTER TABLE test_part_col DROP COLUMN f;

ALTER TABLE ONLY test_part_col_1_prt_other_b ADD COLUMN f int;
ALTER TABLE test_part_col_1_prt_other_b ADD COLUMN f int;

ALTER TABLE ONLY test_part_col_1_prt_other_b ALTER COLUMN e TYPE TEXT; --error
ALTER TABLE test_part_col_1_prt_other_b ALTER COLUMN e TYPE TEXT;

ALTER TABLE ONLY test_part_col_1_prt_other_b DROP COLUMN e; --error
ALTER TABLE test_part_col_1_prt_other_b DROP COLUMN e;

DROP TABLE test_part_col;

-- Non-partition AO table
CREATE TABLE test_part_col(a int, b int, c int, d int, e int)
WITH (appendonly=true) DISTRIBUTED BY(a);

ALTER TABLE ONLY test_part_col ADD COLUMN f int;
ALTER TABLE ONLY test_part_col ALTER COLUMN f TYPE TEXT;
ALTER TABLE ONLY test_part_col DROP COLUMN f;

ALTER TABLE test_part_col ADD COLUMN f int;
ALTER TABLE test_part_col ALTER COLUMN f TYPE TEXT;
ALTER TABLE test_part_col DROP COLUMN f;

DROP TABLE test_part_col;

-- AOCS table
CREATE TABLE test_part_col(a int, b int, c int, d int, e int)
WITH (appendonly=true, orientation=column)
DISTRIBUTED BY(a)
PARTITION BY RANGE (b)
( START (1) END (2) EVERY (1),
    DEFAULT PARTITION other_b);

ALTER TABLE ONLY test_part_col ADD COLUMN f int; --error
ALTER TABLE test_part_col ADD COLUMN f int;

ALTER TABLE ONLY test_part_col ALTER COLUMN f TYPE TEXT; --error
ALTER TABLE test_part_col ALTER COLUMN f TYPE TEXT;

ALTER TABLE ONLY test_part_col DROP COLUMN f; --error
ALTER TABLE test_part_col DROP COLUMN f;

ALTER TABLE ONLY test_part_col_1_prt_other_b ADD COLUMN f int;
ALTER TABLE test_part_col_1_prt_other_b ADD COLUMN f int;

ALTER TABLE ONLY test_part_col_1_prt_other_b ALTER COLUMN e TYPE TEXT; --error
ALTER TABLE test_part_col_1_prt_other_b ALTER COLUMN e TYPE TEXT;

ALTER TABLE ONLY test_part_col_1_prt_other_b DROP COLUMN e; --error
ALTER TABLE test_part_col_1_prt_other_b DROP COLUMN e;

DROP TABLE test_part_col;

-- Non-partition AOCS table
CREATE TABLE test_part_col(a int, b int, c int, d int, e int)
WITH (appendonly=true, orientation=column) DISTRIBUTED BY(a);

ALTER TABLE ONLY test_part_col ADD COLUMN f int;
ALTER TABLE ONLY test_part_col ALTER COLUMN f TYPE TEXT;
ALTER TABLE ONLY test_part_col DROP COLUMN f;

ALTER TABLE test_part_col ADD COLUMN f int;
ALTER TABLE test_part_col ALTER COLUMN f TYPE TEXT;
ALTER TABLE test_part_col DROP COLUMN f;

DROP TABLE test_part_col;


-- Test the new "fast default" mechanism from PostgreSQL v11. It's important
-- that each segment gets the same attmissingval.
CREATE TABLE gp_test_fast_def(i int);
INSERT INTO gp_test_fast_def (i) SELECT g FROM generate_series(1, 10) g;
ALTER TABLE gp_test_fast_def ADD COLUMN ts timestamp DEFAULT now();
ANALYZE gp_test_fast_def;
SELECT COUNT (DISTINCT ts) FROM gp_test_fast_def;


-- Create view with JOIN clause, drop column, check select to view not causing segfault
CREATE TABLE dropped_col_t1(i1 int, i2 int);
CREATE TABLE dropped_col_t2(i1 int, i2 int);
CREATE VIEW dropped_col_v AS SELECT dropped_col_t1.i1 FROM dropped_col_t1 JOIN dropped_col_t2 ON dropped_col_t1.i1=dropped_col_t2.i1;
ALTER TABLE dropped_col_t1 DROP COLUMN i2;
SELECT * FROM dropped_col_v;

-- Test that we are able to attach a newly created partition table when it has foreign key reference.
CREATE TABLE issue_14279_fk_reference (col2 text unique not null);
INSERT INTO issue_14279_fk_reference VALUES ('stuff');
CREATE TABLE issue_14279_taptest_table (
    col1 BIGINT,
    col2 TEXT NOT NULL DEFAULT 'stuff', FOREIGN KEY (col2) REFERENCES issue_14279_fk_reference(col2))
  PARTITION BY RANGE (col1);
CREATE TABLE issue_14279_taptest_table_p3000000000 (
    LIKE issue_14279_taptest_table
    INCLUDING DEFAULTS INCLUDING CONSTRAINTS);
ALTER TABLE issue_14279_taptest_table ATTACH PARTITION issue_14279_taptest_table_p3000000000 FOR VALUES FROM (3000000000) TO (3000000100);
BEGIN;
CREATE TABLE issue_14279_taptest_table_p3000000100 (
    LIKE issue_14279_taptest_table
    INCLUDING DEFAULTS INCLUDING CONSTRAINTS);
ALTER TABLE issue_14279_taptest_table ATTACH PARTITION issue_14279_taptest_table_p3000000100 FOR VALUES FROM (3000000100) TO (3000000200);
END;
INSERT INTO issue_14279_taptest_table SELECT generate_series(3000000000, 3000000001);
INSERT INTO issue_14279_taptest_table SELECT generate_series(3000000100, 3000000101);
-- The parent table shouldn't have anything.
SELECT * FROM ONLY issue_14279_taptest_table;
-- The newly attached table should have 2 rows.
SELECT * FROM issue_14279_taptest_table_p3000000000;
-- The newly attached table should have 2 rows.
SELECT * FROM issue_14279_taptest_table_p3000000100;
DROP TABLE issue_14279_taptest_table;
DROP TABLE issue_14279_fk_reference;
