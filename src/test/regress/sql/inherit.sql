--
-- Test inheritance features
--
CREATE TABLE a (aa TEXT);
CREATE TABLE b (bb TEXT) INHERITS (a);
CREATE TABLE c (cc TEXT) INHERITS (a);
CREATE TABLE d (dd TEXT) INHERITS (b,c,a);

INSERT INTO a(aa) VALUES('aaa');
INSERT INTO a(aa) VALUES('aaaa');
INSERT INTO a(aa) VALUES('aaaaa');
INSERT INTO a(aa) VALUES('aaaaaa');
INSERT INTO a(aa) VALUES('aaaaaaa');
INSERT INTO a(aa) VALUES('aaaaaaaa');

INSERT INTO b(aa) VALUES('bbb');
INSERT INTO b(aa) VALUES('bbbb');
INSERT INTO b(aa) VALUES('bbbbb');
INSERT INTO b(aa) VALUES('bbbbbb');
INSERT INTO b(aa) VALUES('bbbbbbb');
INSERT INTO b(aa) VALUES('bbbbbbbb');

INSERT INTO c(aa) VALUES('ccc');
INSERT INTO c(aa) VALUES('cccc');
INSERT INTO c(aa) VALUES('ccccc');
INSERT INTO c(aa) VALUES('cccccc');
INSERT INTO c(aa) VALUES('ccccccc');
INSERT INTO c(aa) VALUES('cccccccc');

INSERT INTO d(aa) VALUES('ddd');
INSERT INTO d(aa) VALUES('dddd');
INSERT INTO d(aa) VALUES('ddddd');
INSERT INTO d(aa) VALUES('dddddd');
INSERT INTO d(aa) VALUES('ddddddd');
INSERT INTO d(aa) VALUES('dddddddd');

SELECT relname, a.* FROM a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM d, pg_class where d.tableoid = pg_class.oid;
SELECT relname, a.* FROM ONLY a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM ONLY b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM ONLY c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM ONLY d, pg_class where d.tableoid = pg_class.oid;

UPDATE a SET aa='zzzz' WHERE aa='aaaa';
UPDATE ONLY a SET aa='zzzzz' WHERE aa='aaaaa';
UPDATE b SET aa='zzz' WHERE aa='aaa';
UPDATE ONLY b SET aa='zzz' WHERE aa='aaa';
UPDATE a SET aa='zzzzzz' WHERE aa LIKE 'aaa%';

SELECT relname, a.* FROM a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM d, pg_class where d.tableoid = pg_class.oid;
SELECT relname, a.* FROM ONLY a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM ONLY b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM ONLY c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM ONLY d, pg_class where d.tableoid = pg_class.oid;

UPDATE b SET aa='new';

SELECT relname, a.* FROM a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM d, pg_class where d.tableoid = pg_class.oid;
SELECT relname, a.* FROM ONLY a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM ONLY b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM ONLY c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM ONLY d, pg_class where d.tableoid = pg_class.oid;

UPDATE a SET aa='new';

DELETE FROM ONLY c WHERE aa='new';

SELECT relname, a.* FROM a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM d, pg_class where d.tableoid = pg_class.oid;
SELECT relname, a.* FROM ONLY a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM ONLY b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM ONLY c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM ONLY d, pg_class where d.tableoid = pg_class.oid;

DELETE FROM a;

SELECT relname, a.* FROM a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM d, pg_class where d.tableoid = pg_class.oid;
SELECT relname, a.* FROM ONLY a, pg_class where a.tableoid = pg_class.oid;
SELECT relname, b.* FROM ONLY b, pg_class where b.tableoid = pg_class.oid;
SELECT relname, c.* FROM ONLY c, pg_class where c.tableoid = pg_class.oid;
SELECT relname, d.* FROM ONLY d, pg_class where d.tableoid = pg_class.oid;

-- Confirm PRIMARY KEY adds NOT NULL constraint to child table
CREATE TEMP TABLE z (b TEXT, PRIMARY KEY(aa, b)) inherits (a);
INSERT INTO z VALUES (NULL, 'text'); -- should fail

-- Check inherited UPDATE with all children excluded
create table some_tab (a int, b int);
create table some_tab_child () inherits (some_tab);
insert into some_tab_child values(1,2);

explain (verbose, costs off)
update some_tab set a = a + 1 where false;
update some_tab set a = a + 1 where false;
explain (verbose, costs off)
update some_tab set a = a + 1 where false returning b, a;
update some_tab set a = a + 1 where false returning b, a;
table some_tab;

drop table some_tab cascade;

-- Check UPDATE with inherited target and an inherited source table
create temp table foo(f1 int, f2 int);
create temp table foo2(f3 int) inherits (foo);
create temp table bar(f1 int, f2 int);
create temp table bar2(f3 int) inherits (bar);

insert into foo values(1,1);
insert into foo values(3,3);
insert into foo2 values(2,2,2);
insert into foo2 values(3,3,3);
insert into bar values(1,1);
insert into bar values(2,2);
insert into bar values(3,3);
insert into bar values(4,4);
insert into bar2 values(1,1,1);
insert into bar2 values(2,2,2);
insert into bar2 values(3,3,3);
insert into bar2 values(4,4,4);

update bar set f2 = f2 + 100 where f1 in (select f1 from foo);

select tableoid::regclass::text as relname, bar.* from bar order by 1,2;

-- Check UPDATE with inherited target and an appendrel subquery
update bar set f2 = f2 + 100
from
  ( select f1 from foo union all select f1+3 from foo ) ss
where bar.f1 = ss.f1;

select tableoid::regclass::text as relname, bar.* from bar order by 1,2;

-- Check UPDATE with *partitioned* inherited target and an appendrel subquery
create table some_tab (a int);
insert into some_tab values (0);
create table some_tab_child () inherits (some_tab);
insert into some_tab_child values (1);
create table parted_tab (a int, b char) partition by list (a);
create table parted_tab_part1 partition of parted_tab for values in (1);
create table parted_tab_part2 partition of parted_tab for values in (2);
create table parted_tab_part3 partition of parted_tab for values in (3);
insert into parted_tab values (1, 'a'), (2, 'a'), (3, 'a');

update parted_tab set b = 'b'
from
  (select a from some_tab union all select a+1 from some_tab) ss (a)
where parted_tab.a = ss.a;
select tableoid::regclass::text as relname, parted_tab.* from parted_tab order by 1,2;

truncate parted_tab;
insert into parted_tab values (1, 'a'), (2, 'a'), (3, 'a');
update parted_tab set b = 'b'
from
  (select 0 from parted_tab union all select 1 from parted_tab) ss (a)
where parted_tab.a = ss.a;
select tableoid::regclass::text as relname, parted_tab.* from parted_tab order by 1,2;

-- modifies partition key, but no rows will actually be updated
explain update parted_tab set a = 2 where false;

drop table parted_tab;

-- Check UPDATE with multi-level partitioned inherited target
create table mlparted_tab (a int, b char, c text) partition by list (a);
create table mlparted_tab_part1 partition of mlparted_tab for values in (1);
create table mlparted_tab_part2 partition of mlparted_tab for values in (2) partition by list (b);
create table mlparted_tab_part3 partition of mlparted_tab for values in (3);
create table mlparted_tab_part2a partition of mlparted_tab_part2 for values in ('a');
create table mlparted_tab_part2b partition of mlparted_tab_part2 for values in ('b');
insert into mlparted_tab values (1, 'a'), (2, 'a'), (2, 'b'), (3, 'a');

update mlparted_tab mlp set c = 'xxx'
from
  (select a from some_tab union all select a+1 from some_tab) ss (a)
where (mlp.a = ss.a and mlp.b = 'b') or mlp.a = 3;
select tableoid::regclass::text as relname, mlparted_tab.* from mlparted_tab order by 1,2;

drop table mlparted_tab;
drop table some_tab cascade;

/* Test multiple inheritance of column defaults */

CREATE TABLE firstparent (tomorrow date default now()::date + 1);
CREATE TABLE secondparent (tomorrow date default  now() :: date  +  1);
CREATE TABLE jointchild () INHERITS (firstparent, secondparent);  -- ok
CREATE TABLE thirdparent (tomorrow date default now()::date - 1);
CREATE TABLE otherchild () INHERITS (firstparent, thirdparent);  -- not ok
CREATE TABLE otherchild (tomorrow date default now())
  INHERITS (firstparent, thirdparent);  -- ok, child resolves ambiguous default

DROP TABLE firstparent, secondparent, jointchild, thirdparent, otherchild;

-- Test changing the type of inherited columns
insert into d values('test','one','two','three');
alter table z drop constraint z_pkey;
alter table a alter column aa type integer using bit_length(aa);
-- In GPDB, the table is distributed by the 'aa' column, changing its type
-- therefore fails. Change the distribution key and try again.
alter table a set distributed randomly;
alter table a alter column aa type integer using bit_length(aa);
select * from d;

-- Test non-inheritable parent constraints
create table p1(ff1 int);
alter table p1 add constraint p1chk check (ff1 > 0) no inherit;
alter table p1 add constraint p2chk check (ff1 > 10);
-- connoinherit should be true for NO INHERIT constraint
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pgc.connoinherit from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname = 'p1' order by 1,2;

-- Test that child does not inherit NO INHERIT constraints
create table c1 () inherits (p1);
\d p1
\d c1

-- Test that child does not override inheritable constraints of the parent
create table c2 (constraint p2chk check (ff1 > 10) no inherit) inherits (p1);	--fails

drop table p1 cascade;

-- Tests for casting between the rowtypes of parent and child
-- tables. See the pgsql-hackers thread beginning Dec. 4/04
create table base (i integer);
create table derived () inherits (base);
create table more_derived (like derived, b int) inherits (derived);
insert into derived (i) values (0);
select derived::base from derived;
select NULL::derived::base;
-- remove redundant conversions.
explain (verbose on, costs off) select row(i, b)::more_derived::derived::base from more_derived;
explain (verbose on, costs off) select (1, 2)::more_derived::derived::base;
drop table more_derived;
drop table derived;
drop table base;

create table p1(ff1 int);
create table p2(f1 text);
create function p2text(p2) returns text as 'select $1.f1' language sql;
create table c1(f3 int) inherits(p1,p2);
insert into c1 values(123456789, 'hi', 42);
select p2text(c1.*) from c1;
drop function p2text(p2);
drop table c1;
drop table p2;
drop table p1;

CREATE TABLE ac (aa TEXT);
alter table ac add constraint ac_check check (aa is not null);
CREATE TABLE bc (bb TEXT) INHERITS (ac);
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pg_get_expr(pgc.conbin, pc.oid) as consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

insert into ac (aa) values (NULL);
insert into bc (aa) values (NULL);

alter table bc drop constraint ac_check;  -- fail, disallowed
alter table ac drop constraint ac_check;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pg_get_expr(pgc.conbin, pc.oid) as consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

-- try the unnamed-constraint case
alter table ac add check (aa is not null);
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pg_get_expr(pgc.conbin, pc.oid) as consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

insert into ac (aa) values (NULL);
insert into bc (aa) values (NULL);

alter table bc drop constraint ac_aa_check;  -- fail, disallowed
alter table ac drop constraint ac_aa_check;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pg_get_expr(pgc.conbin, pc.oid) as consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

alter table ac add constraint ac_check check (aa is not null);
alter table bc no inherit ac;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pg_get_expr(pgc.conbin, pc.oid) as consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;
alter table bc drop constraint ac_check;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pg_get_expr(pgc.conbin, pc.oid) as consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;
alter table ac drop constraint ac_check;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pg_get_expr(pgc.conbin, pc.oid) as consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

drop table bc;
drop table ac;

create table ac (a int constraint check_a check (a <> 0));
create table bc (a int constraint check_a check (a <> 0), b int constraint check_b check (b <> 0)) inherits (ac);
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pg_get_expr(pgc.conbin, pc.oid) as consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc') order by 1,2;

drop table bc;
drop table ac;

create table ac (a int constraint check_a check (a <> 0));
create table bc (b int constraint check_b check (b <> 0));
create table cc (c int constraint check_c check (c <> 0)) inherits (ac, bc);
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pg_get_expr(pgc.conbin, pc.oid) as consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc', 'cc') order by 1,2;

alter table cc no inherit bc;
select pc.relname, pgc.conname, pgc.contype, pgc.conislocal, pgc.coninhcount, pg_get_expr(pgc.conbin, pc.oid) as consrc from pg_class as pc inner join pg_constraint as pgc on (pgc.conrelid = pc.oid) where pc.relname in ('ac', 'bc', 'cc') order by 1,2;

drop table cc;
drop table bc;
drop table ac;

create table p1(f1 int);
create table p2(f2 int);
create table c1(f3 int) inherits(p1,p2);
insert into c1 values(1,-1,2);
alter table p2 add constraint cc check (f2>0);  -- fail
alter table p2 add check (f2>0);  -- check it without a name, too
delete from c1;
insert into c1 values(1,1,2);
alter table p2 add check (f2>0);
insert into c1 values(1,-1,2);  -- fail
create table c2(f3 int) inherits(p1,p2);
\d c2
create table c3 (f4 int) inherits(c1,c2);
\d c3
drop table p1 cascade;
drop table p2 cascade;

create table pp1 (f1 int);
create table cc1 (f2 text, f3 int) inherits (pp1);
alter table pp1 add column a1 int check (a1 > 0);
\d cc1
create table cc2(f4 float) inherits(pp1,cc1);
\d cc2
alter table pp1 add column a2 int check (a2 > 0);
\d cc2
drop table pp1 cascade;

-- Test for renaming in simple multiple inheritance
CREATE TABLE inht1 (a int, b int);
CREATE TABLE inhs1 (b int, c int);
CREATE TABLE inhts (d int) INHERITS (inht1, inhs1);

ALTER TABLE inht1 RENAME a TO aa;
ALTER TABLE inht1 RENAME b TO bb;                -- to be failed
ALTER TABLE inhts RENAME aa TO aaa;      -- to be failed
ALTER TABLE inhts RENAME d TO dd;
\d+ inhts

DROP TABLE inhts;

-- Test for renaming in diamond inheritance
CREATE TABLE inht2 (x int) INHERITS (inht1);
CREATE TABLE inht3 (y int) INHERITS (inht1);
CREATE TABLE inht4 (z int) INHERITS (inht2, inht3);

ALTER TABLE inht1 RENAME aa TO aaa;
\d+ inht4

CREATE TABLE inhts (d int) INHERITS (inht2, inhs1);
ALTER TABLE inht1 RENAME aaa TO aaaa;
ALTER TABLE inht1 RENAME b TO bb;                -- to be failed
\d+ inhts

WITH RECURSIVE r AS (
  SELECT 'inht1'::regclass AS inhrelid
UNION ALL
  SELECT c.inhrelid FROM pg_inherits c, r WHERE r.inhrelid = c.inhparent
)
SELECT a.attrelid::regclass, a.attname, a.attinhcount, e.expected
  FROM (SELECT inhrelid, count(*) AS expected FROM pg_inherits
        WHERE inhparent IN (SELECT inhrelid FROM r) GROUP BY inhrelid) e
  JOIN pg_attribute a ON e.inhrelid = a.attrelid WHERE NOT attislocal
  ORDER BY a.attrelid::regclass::name, a.attnum;

DROP TABLE inht1, inhs1 CASCADE;


-- Test non-inheritable indices [UNIQUE, EXCLUDE] constraints
CREATE TABLE test_constraints (id int, val1 varchar, val2 int, UNIQUE(val1, val2));
CREATE TABLE test_constraints_inh () INHERITS (test_constraints);
\d+ test_constraints
ALTER TABLE ONLY test_constraints DROP CONSTRAINT test_constraints_val1_val2_key;
\d+ test_constraints
\d+ test_constraints_inh
DROP TABLE test_constraints_inh;
DROP TABLE test_constraints;

CREATE TABLE test_ex_constraints (
    c circle,
    dkey inet,
    EXCLUDE USING gist (dkey inet_ops WITH =, c WITH &&)
);

CREATE TABLE test_ex_constraints_inh () INHERITS (test_ex_constraints);
\d+ test_ex_constraints
ALTER TABLE test_ex_constraints DROP CONSTRAINT test_ex_constraints_dkey_c_excl;
\d+ test_ex_constraints
\d+ test_ex_constraints_inh
DROP TABLE test_ex_constraints_inh;
DROP TABLE test_ex_constraints;

-- Test non-inheritable foreign key constraints
CREATE TABLE test_primary_constraints(id int PRIMARY KEY);
CREATE TABLE test_foreign_constraints(id1 int REFERENCES test_primary_constraints(id));
CREATE TABLE test_foreign_constraints_inh () INHERITS (test_foreign_constraints);
\d+ test_primary_constraints
\d+ test_foreign_constraints
ALTER TABLE test_foreign_constraints DROP CONSTRAINT test_foreign_constraints_id1_fkey;
\d+ test_foreign_constraints
\d+ test_foreign_constraints_inh
DROP TABLE test_foreign_constraints_inh;
DROP TABLE test_foreign_constraints;
DROP TABLE test_primary_constraints;

-- Test foreign key behavior
create table inh_fk_1 (a int primary key);
insert into inh_fk_1 values (1), (2), (3);
create table inh_fk_2 (x int primary key, y int references inh_fk_1 on delete cascade);
insert into inh_fk_2 values (11, 1), (22, 2), (33, 3);
create table inh_fk_2_child () inherits (inh_fk_2);
insert into inh_fk_2_child values (111, 1), (222, 2);
-- The cascading deletion doesn't work on GPDB, because foreign keys are not
-- enforced in general. So this produces different result than on upstream.
delete from inh_fk_1 where a = 1;
select * from inh_fk_1 order by 1;
select * from inh_fk_2 order by 1, 2;
drop table inh_fk_1, inh_fk_2, inh_fk_2_child;

-- Test that parent and child CHECK constraints can be created in either order
create table p1(f1 int);
create table p1_c1() inherits(p1);

alter table p1 add constraint inh_check_constraint1 check (f1 > 0);
alter table p1_c1 add constraint inh_check_constraint1 check (f1 > 0);

alter table p1_c1 add constraint inh_check_constraint2 check (f1 < 10);
alter table p1 add constraint inh_check_constraint2 check (f1 < 10);

select conrelid::regclass::text as relname, conname, conislocal, coninhcount
from pg_constraint where conname like 'inh\_check\_constraint%'
order by 1, 2;

drop table p1 cascade;

-- Test that a valid child can have not-valid parent, but not vice versa
create table invalid_check_con(f1 int);
create table invalid_check_con_child() inherits(invalid_check_con);

alter table invalid_check_con_child add constraint inh_check_constraint check(f1 > 0) not valid;
alter table invalid_check_con add constraint inh_check_constraint check(f1 > 0); -- fail
alter table invalid_check_con_child drop constraint inh_check_constraint;

insert into invalid_check_con values(0);

alter table invalid_check_con_child add constraint inh_check_constraint check(f1 > 0);
alter table invalid_check_con add constraint inh_check_constraint check(f1 > 0) not valid;

insert into invalid_check_con values(0); -- fail
insert into invalid_check_con_child values(0); -- fail

select conrelid::regclass::text as relname, conname,
       convalidated, conislocal, coninhcount, connoinherit
from pg_constraint where conname like 'inh\_check\_constraint%'
order by 1, 2;

-- We don't drop the invalid_check_con* tables, to test dump/reload with

--
-- Test parameterized append plans for inheritance trees
--

create temp table patest0 (id, x) as
  select x, x from generate_series(0,1000) x
  distributed by (id);
create temp table patest1() inherits (patest0);
insert into patest1
  select x, x from generate_series(0,1000) x;
create temp table patest2() inherits (patest0);
insert into patest2
  select x, x from generate_series(0,1000) x;
create index patest0i on patest0(id);
create index patest1i on patest1(id);
create index patest2i on patest2(id);
analyze patest0;
analyze patest1;
analyze patest2;

set enable_seqscan=off;
set enable_bitmapscan=off;
explain (costs off)
select * from patest0 join (select f1 from int4_tbl where f1 < 10 and f1 > -10 limit 1) ss on id = f1;
select * from patest0 join (select f1 from int4_tbl where f1 < 10 and f1 > -10 limit 1) ss on id = f1;

drop index patest2i;

explain (costs off)
select * from patest0 join (select f1 from int4_tbl where f1 < 10 and f1 > -10 limit 1) ss on id = f1;
select * from patest0 join (select f1 from int4_tbl where f1 < 10 and f1 > -10 limit 1) ss on id = f1;
reset enable_seqscan;
reset enable_bitmapscan;

drop table patest0 cascade;

--
-- Test merge-append plans for inheritance trees
--

create table matest0 (id serial primary key, name text);
create table matest1 (id integer primary key) inherits (matest0);
create table matest2 (id integer primary key) inherits (matest0);
create table matest3 (id integer primary key) inherits (matest0);

create index matest0i on matest0 ((1-id));
create index matest1i on matest1 ((1-id));
-- create index matest2i on matest2 ((1-id));  -- intentionally missing
create index matest3i on matest3 ((1-id));

insert into matest1 (name) values ('Test 1');
insert into matest1 (name) values ('Test 2');
insert into matest2 (name) values ('Test 3');
insert into matest2 (name) values ('Test 4');
insert into matest3 (name) values ('Test 5');
insert into matest3 (name) values ('Test 6');

set enable_indexscan = off;  -- force use of seqscan/sort, so no merge
explain (verbose, costs off) select * from matest0 order by 1-id;
select * from matest0 order by 1-id;
explain (verbose, costs off) select min(1-id) from matest0;
select min(1-id) from matest0;
reset enable_indexscan;

set enable_seqscan = off;  -- plan with fewest seqscans should be merge
set enable_parallel_append = off; -- Don't let parallel-append interfere
-- GPDB_92_MERGE_FIXME: the cost of bitmap scan is not correct?
-- the cost of merge append with index scan is bigger than the cost
-- of append with bitmapscan + sort
set enable_bitmapscan = off; 
explain (verbose, costs off) select * from matest0 order by 1-id;
select * from matest0 order by 1-id;
explain (verbose, costs off) select min(1-id) from matest0;
select min(1-id) from matest0;
reset enable_seqscan;
reset enable_parallel_append;
reset enable_bitmapscan;

drop table matest0 cascade;

--
-- Check that use of an index with an extraneous column doesn't produce
-- a plan with extraneous sorting
--

create table matest0 (a int, b int, c int, d int);
create table matest1 () inherits(matest0);
create index matest0i on matest0 (b, c);
create index matest1i on matest1 (b, c);

set enable_nestloop = off;  -- we want a plan with two MergeAppends
set enable_mergejoin=on;

explain (costs off)
select t1.* from matest0 t1, matest0 t2
where t1.b = t2.b and t2.c = t2.d
order by t1.b limit 10;

reset enable_nestloop;

drop table matest0 cascade;

--
-- Test merge-append for UNION ALL append relations
--

set enable_seqscan = off;
set enable_indexscan = on;
set enable_bitmapscan = off;

-- GPDB: coerce the planner to choose Merge Append plans for the below queries.
-- In upstream, the Merge Append is cheaper, but in GPDB the Sort within each
-- segment only has to sort 1 / 3 of the data (with three segments), making
-- Sort + Append cheaper. Compensate by pretending that there are more rows in
-- the table.
begin;
set allow_system_table_mods = on;
update pg_class set reltuples = 100000 where oid = 'tenk1'::regclass;

-- Check handling of duplicated, constant, or volatile targetlist items
explain (costs off)
SELECT thousand, tenthous FROM tenk1
UNION ALL
SELECT thousand, thousand FROM tenk1
ORDER BY thousand, tenthous;

explain (costs off)
SELECT thousand, tenthous, thousand+tenthous AS x FROM tenk1
UNION ALL
SELECT 42, 42, hundred FROM tenk1
ORDER BY thousand, tenthous;

explain (costs off)
SELECT thousand, tenthous FROM tenk1
UNION ALL
SELECT thousand, random()::integer FROM tenk1
ORDER BY thousand, tenthous;

-- Check min/max aggregate optimization
explain (costs off)
SELECT min(x) FROM
  (SELECT unique1 AS x FROM tenk1 a
   UNION ALL
   SELECT unique2 AS x FROM tenk1 b) s;

explain (costs off)
SELECT min(y) FROM
  (SELECT unique1 AS x, unique1 AS y FROM tenk1 a
   UNION ALL
   SELECT unique2 AS x, unique2 AS y FROM tenk1 b) s;

-- XXX planner doesn't recognize that index on unique2 is sufficiently sorted
explain (costs off)
SELECT x, y FROM
  (SELECT thousand AS x, tenthous AS y FROM tenk1 a
   UNION ALL
   SELECT unique2 AS x, unique2 AS y FROM tenk1 b) s
ORDER BY x, y;

-- exercise rescan code path via a repeatedly-evaluated subquery
explain (costs off)
SELECT
    ARRAY(SELECT f.i FROM (
        (SELECT d + g.i FROM generate_series(4, 30, 3) d ORDER BY 1)
        UNION ALL
        (SELECT d + g.i FROM generate_series(0, 30, 5) d ORDER BY 1)
    ) f(i)
    ORDER BY f.i LIMIT 10)
FROM generate_series(1, 3) g(i);

SELECT
    ARRAY(SELECT f.i FROM (
        (SELECT d + g.i FROM generate_series(4, 30, 3) d ORDER BY 1)
        UNION ALL
        (SELECT d + g.i FROM generate_series(0, 30, 5) d ORDER BY 1)
    ) f(i)
    ORDER BY f.i LIMIT 10)
FROM generate_series(1, 3) g(i);

reset enable_seqscan;
reset enable_indexscan;
reset enable_bitmapscan;

rollback;

--
-- Check handling of a constant-null CHECK constraint
--
create table cnullparent (f1 int);
create table cnullchild (check (f1 = 1 or f1 = null)) inherits(cnullparent);
insert into cnullchild values(1);
insert into cnullchild values(2);
insert into cnullchild values(null);
select * from cnullparent;
select * from cnullparent where f1 = 2;
drop table cnullparent cascade;

--
-- Check use of temporary tables with inheritance trees
--
create table inh_perm_parent (a1 int);
create temp table inh_temp_parent (a1 int);
create temp table inh_temp_child () inherits (inh_perm_parent); -- ok
create table inh_perm_child () inherits (inh_temp_parent); -- error
create temp table inh_temp_child_2 () inherits (inh_temp_parent); -- ok
insert into inh_perm_parent values (1);
insert into inh_temp_parent values (2);
insert into inh_temp_child values (3);
insert into inh_temp_child_2 values (4);
select tableoid::regclass, a1 from inh_perm_parent;
select tableoid::regclass, a1 from inh_temp_parent;
drop table inh_perm_parent cascade;
drop table inh_temp_parent cascade;

--
-- Check that constraint exclusion works correctly with partitions using
-- implicit constraints generated from the partition bound information.
--
create table list_parted (
	a	varchar
) partition by list (a);
create table part_ab_cd partition of list_parted for values in ('ab', 'cd');
create table part_ef_gh partition of list_parted for values in ('ef', 'gh');
create table part_null_xy partition of list_parted for values in (null, 'xy');

explain (costs off) select * from list_parted;
explain (costs off) select * from list_parted where a is null;
explain (costs off) select * from list_parted where a is not null;
explain (costs off) select * from list_parted where a in ('ab', 'cd', 'ef');
explain (costs off) select * from list_parted where a = 'ab' or a in (null, 'cd');
explain (costs off) select * from list_parted where a = 'ab';

create table range_list_parted (
	a	int,
	b	char(2)
) partition by range (a);
create table part_1_10 partition of range_list_parted for values from (1) to (10) partition by list (b);
create table part_1_10_ab partition of part_1_10 for values in ('ab');
create table part_1_10_cd partition of part_1_10 for values in ('cd');
create table part_10_20 partition of range_list_parted for values from (10) to (20) partition by list (b);
create table part_10_20_ab partition of part_10_20 for values in ('ab');
create table part_10_20_cd partition of part_10_20 for values in ('cd');
create table part_21_30 partition of range_list_parted for values from (21) to (30) partition by list (b);
create table part_21_30_ab partition of part_21_30 for values in ('ab');
create table part_21_30_cd partition of part_21_30 for values in ('cd');
create table part_40_inf partition of range_list_parted for values from (40) to (maxvalue) partition by list (b);
create table part_40_inf_ab partition of part_40_inf for values in ('ab');
create table part_40_inf_cd partition of part_40_inf for values in ('cd');
create table part_40_inf_null partition of part_40_inf for values in (null);

explain (costs off) select * from range_list_parted;
explain (costs off) select * from range_list_parted where a = 5;
explain (costs off) select * from range_list_parted where b = 'ab';
explain (costs off) select * from range_list_parted where a between 3 and 23 and b in ('ab');

/* Should select no rows because range partition key cannot be null */
explain (costs off) select * from range_list_parted where a is null;

/* Should only select rows from the null-accepting partition */
explain (costs off) select * from range_list_parted where b is null;
explain (costs off) select * from range_list_parted where a is not null and a < 67;
explain (costs off) select * from range_list_parted where a >= 30;

drop table list_parted;
drop table range_list_parted;

-- check that constraint exclusion is able to cope with the partition
-- constraint emitted for multi-column range partitioned tables
create table mcrparted (a int, b int, c int) partition by range (a, abs(b), c);
create table mcrparted_def partition of mcrparted default;
create table mcrparted0 partition of mcrparted for values from (minvalue, minvalue, minvalue) to (1, 1, 1);
create table mcrparted1 partition of mcrparted for values from (1, 1, 1) to (10, 5, 10);
create table mcrparted2 partition of mcrparted for values from (10, 5, 10) to (10, 10, 10);
create table mcrparted3 partition of mcrparted for values from (11, 1, 1) to (20, 10, 10);
create table mcrparted4 partition of mcrparted for values from (20, 10, 10) to (20, 20, 20);
create table mcrparted5 partition of mcrparted for values from (20, 20, 20) to (maxvalue, maxvalue, maxvalue);
explain (costs off) select * from mcrparted where a = 0;	-- scans mcrparted0, mcrparted_def
explain (costs off) select * from mcrparted where a = 10 and abs(b) < 5;	-- scans mcrparted1, mcrparted_def
explain (costs off) select * from mcrparted where a = 10 and abs(b) = 5;	-- scans mcrparted1, mcrparted2, mcrparted_def
explain (costs off) select * from mcrparted where abs(b) = 5;	-- scans all partitions
explain (costs off) select * from mcrparted where a > -1;	-- scans all partitions
explain (costs off) select * from mcrparted where a = 20 and abs(b) = 10 and c > 10;	-- scans mcrparted4
explain (costs off) select * from mcrparted where a = 20 and c > 20; -- scans mcrparted3, mcrparte4, mcrparte5, mcrparted_def

-- check that partitioned table Appends cope with being referenced in
-- subplans
create table parted_minmax (a int, b varchar(16)) partition by range (a);
create table parted_minmax1 partition of parted_minmax for values from (1) to (10);
create index parted_minmax1i on parted_minmax1 (a, b);
insert into parted_minmax values (1,'12345');
explain (costs off) select min(a), max(a) from parted_minmax where b = '12345';
select min(a), max(a) from parted_minmax where b = '12345';
drop table parted_minmax;

-- Test code that uses Append nodes in place of MergeAppend when the
-- partition ordering matches the desired ordering.

create index mcrparted_a_abs_c_idx on mcrparted (a, abs(b), c);

-- MergeAppend must be used when a default partition exists
explain (costs off) select * from mcrparted order by a, abs(b), c;

drop table mcrparted_def;

-- Append is used for a RANGE partitioned table with no default
-- and no subpartitions
explain (costs off) select * from mcrparted order by a, abs(b), c;

-- Append is used with subpaths in reverse order with backwards index scans
explain (costs off) select * from mcrparted order by a desc, abs(b) desc, c desc;

-- check that Append plan is used containing a MergeAppend for sub-partitions
-- that are unordered.
drop table mcrparted5;
create table mcrparted5 partition of mcrparted for values from (20, 20, 20) to (maxvalue, maxvalue, maxvalue) partition by list (a);
create table mcrparted5a partition of mcrparted5 for values in(20);
create table mcrparted5_def partition of mcrparted5 default;

explain (costs off) select * from mcrparted order by a, abs(b), c;

drop table mcrparted5_def;

-- check that an Append plan is used and the sub-partitions are flattened
-- into the main Append when the sub-partition is unordered but contains
-- just a single sub-partition.
explain (costs off) select a, abs(b) from mcrparted order by a, abs(b), c;

-- check that Append is used when the sub-partitioned tables are pruned
-- during planning.
explain (costs off) select * from mcrparted where a < 20 order by a, abs(b), c;

create table mclparted (a int) partition by list(a);
create table mclparted1 partition of mclparted for values in(1);
create table mclparted2 partition of mclparted for values in(2);
create index on mclparted (a);

-- Ensure an Append is used for a list partition with an order by.
explain (costs off) select * from mclparted order by a;

-- Ensure a MergeAppend is used when a partition exists with interleaved
-- datums in the partition bound.
create table mclparted3_5 partition of mclparted for values in(3,5);
create table mclparted4 partition of mclparted for values in(4);

explain (costs off) select * from mclparted order by a;

drop table mclparted;

-- Ensure subplans which don't have a path with the correct pathkeys get
-- sorted correctly.
drop index mcrparted_a_abs_c_idx;
create index on mcrparted1 (a, abs(b), c);
create index on mcrparted2 (a, abs(b), c);
create index on mcrparted3 (a, abs(b), c);
create index on mcrparted4 (a, abs(b), c);

explain (costs off) select * from mcrparted where a < 20 order by a, abs(b), c limit 1;

set enable_bitmapscan = 0;
-- Ensure Append node can be used when the partition is ordered by some
-- pathkeys which were deemed redundant.
explain (costs off) select * from mcrparted where a = 10 order by a, abs(b), c;
reset enable_bitmapscan;

drop table mcrparted;

-- Ensure LIST partitions allow an Append to be used instead of a MergeAppend
create table bool_lp (b bool) partition by list(b);
create table bool_lp_true partition of bool_lp for values in(true);
create table bool_lp_false partition of bool_lp for values in(false);
create index on bool_lp (b);

explain (costs off) select * from bool_lp order by b;

drop table bool_lp;

-- Ensure const bool quals can be properly detected as redundant
create table bool_rp (b bool, a int) partition by range(b,a);
create table bool_rp_false_1k partition of bool_rp for values from (false,0) to (false,1000);
create table bool_rp_true_1k partition of bool_rp for values from (true,0) to (true,1000);
create table bool_rp_false_2k partition of bool_rp for values from (false,1000) to (false,2000);
create table bool_rp_true_2k partition of bool_rp for values from (true,1000) to (true,2000);
create index on bool_rp (b,a);
explain (costs off) select * from bool_rp where b = true order by b,a;
explain (costs off) select * from bool_rp where b = false order by b,a;
-- GPDB: force the planner to choose same plan as in upstream
set enable_seqscan=off;
set enable_bitmapscan=off;
explain (costs off) select * from bool_rp where b = true order by a;
explain (costs off) select * from bool_rp where b = false order by a;
reset enable_seqscan;
reset enable_bitmapscan;

drop table bool_rp;

-- Ensure an Append scan is chosen when the partition order is a subset of
-- the required order.
create table range_parted (a int, b int, c int) partition by range(a, b);
create table range_parted1 partition of range_parted for values from (0,0) to (10,10);
create table range_parted2 partition of range_parted for values from (10,10) to (20,20);
create index on range_parted (a,b,c);

explain (costs off) select * from range_parted order by a,b,c;
explain (costs off) select * from range_parted order by a desc,b desc,c desc;

drop table range_parted;
