-- ALTER TABLE ... RENAME on corrupted relations
SET allow_system_table_mods = dml;
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

-- relname is out of sync
CREATE TABLE cor (a int, b int);
UPDATE pg_class SET relname='othercor' WHERE relname='cor';
ALTER TABLE othercor RENAME TO tmpcor;
ALTER TABLE tmpcor RENAME TO cor;
DROP TABLE cor;

RESET allow_system_table_mods;
RESET gp_allow_rename_relation_without_lock;


-- MPP-20466 Dis-allow duplicate constraint names for same table
create table dupconstr (
						i int,
						j int constraint test CHECK (j > 10))
						distributed by (i);
-- should fail because of duplicate constraint name
alter table dupconstr add constraint test unique (i);
alter table dupconstr add constraint test primary key (i);
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
