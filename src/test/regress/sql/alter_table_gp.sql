-- ALTER TABLE ... RENAME on corrupted relations
SET allow_system_table_mods = true;
SET gp_allow_rename_relation_without_lock = ON;
-- missing entry
CREATE TABLE cor (a int, b float);
INSERT INTO cor SELECT i, i+1 FROM generate_series(1,100)i;
DELETE FROM pg_attribute WHERE attname='a' AND attrelid='cor'::regclass;
ALTER TABLE cor RENAME TO oldcor;
INSERT INTO pg_attribute SELECT distinct on(attrelid, attnum) * FROM gp_dist_random('pg_attribute') WHERE attname='a' AND attrelid='oldcor'::regclass;
DROP TABLE oldcor;

-- typname is out of sync
CREATE TABLE cor (a int, b float, c text);
UPDATE pg_type SET typname='newcor' WHERE typrelid='cor'::regclass;
ALTER TABLE cor RENAME TO newcor2;
ALTER TABLE newcor2 RENAME TO cor;
DROP TABLE cor;

RESET allow_system_table_mods;
RESET gp_allow_rename_relation_without_lock;


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
