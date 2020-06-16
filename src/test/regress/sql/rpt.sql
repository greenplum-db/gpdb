--
-- Basic tests for replicated table
--
create schema rpt;
set search_path to rpt;

---------
-- INSERT
---------
create table foo (x int, y int) distributed replicated;
create table foo1(like foo) distributed replicated;
create table bar (like foo) distributed randomly;
create table bar1 (like foo) distributed by (x);

-- values --> replicated table 
-- random partitioned table --> replicated table
-- hash partitioned table --> replicated table
-- singleQE --> replicated table
-- replicated --> replicated table
insert into bar values (1, 1), (3, 1);
insert into bar1 values (1, 1), (3, 1);
insert into foo1 values (1, 1), (3, 1);
insert into foo select * from bar;
insert into foo select * from bar1;
insert into foo select * from bar order by x limit 1;
insert into foo select * from foo;

select * from foo order by x;
select bar.x, bar.y from bar, (select * from foo) as t1 order by 1,2;
select bar.x, bar.y from bar, (select * from foo order by x limit 1) as t1 order by 1,2;

truncate foo;
truncate foo1;
truncate bar;
truncate bar1;

-- replicated table --> random partitioned table
-- replicated table --> hash partitioned table
insert into foo values (1, 1), (3, 1);
insert into bar select * from foo order by x limit 1;
insert into bar1 select * from foo order by x limit 1;

select * from foo order by x;
select * from bar order by x;
select * from bar1 order by x;

drop table if exists foo;
drop table if exists foo1;
drop table if exists bar;
drop table if exists bar1;

--
-- CREATE UNIQUE INDEX
--
-- create unique index on non-distributed key.
create table foo (x int, y int) distributed replicated;
create table bar (x int, y int) distributed randomly;

-- success
create unique index foo_idx on foo (y);
-- should fail
create unique index bar_idx on bar (y);

drop table if exists foo;
drop table if exists bar;

--
-- CREATE TABLE with both PRIMARY KEY and UNIQUE constraints
--
create table foo (id int primary key, name text unique) distributed replicated;

-- success
insert into foo values (1,'aaa');
insert into foo values (2,'bbb');

-- fail
insert into foo values (1,'ccc');
insert into foo values (3,'aaa');

drop table if exists foo;

--
-- CREATE TABLE
--
--
-- Like
CREATE TABLE parent (
        name            text,
        age                     int4,
        location        point
) distributed replicated;

CREATE TABLE child (like parent) distributed replicated;
CREATE TABLE child1 (like parent) DISTRIBUTED BY (name);
CREATE TABLE child2 (like parent);

-- should be replicated table
\d child
-- should distributed by name
\d child1
-- should be replicated table
\d child2

drop table if exists parent;
drop table if exists child;
drop table if exists child1;
drop table if exists child2;

-- Inherits
CREATE TABLE parent_rep (
        name            text,
        age                     int4,
        location        point
) distributed replicated;

CREATE TABLE parent_part (
        name            text,
        age                     int4,
        location        point
) distributed by (name);

-- inherits from a replicated table, should fail
CREATE TABLE child (
        salary          int4,
        manager         name
) INHERITS (parent_rep) WITH OIDS;

-- replicated table can not have parents, should fail
CREATE TABLE child (
        salary          int4,
        manager         name
) INHERITS (parent_part) WITH OIDS DISTRIBUTED REPLICATED;

drop table if exists parent_rep;
drop table if exists parent_part;
drop table if exists child;

--
-- CTAS
--
-- CTAS from generate_series
create table foo as select i as c1, i as c2
from generate_series(1,3) i distributed replicated;

-- CTAS from replicated table 
create table bar as select * from foo distributed replicated;
select * from bar;

drop table if exists foo;
drop table if exists bar;

-- CTAS from partition table table
create table foo as select i as c1, i as c2
from generate_series(1,3) i;

create table bar as select * from foo distributed replicated;
select * from bar;

drop table if exists foo;
drop table if exists bar;

-- CTAS from singleQE 
create table foo as select i as c1, i as c2
from generate_series(1,3) i;
select * from foo;

create table bar as select * from foo order by c1 limit 1 distributed replicated;
select * from bar;

drop table if exists foo;
drop table if exists bar;

-- Create view can work
create table foo(x int, y int) distributed replicated;
insert into foo values(1,1);

create view v_foo as select * from foo;
select * from v_foo;

drop view v_foo;
drop table if exists foo;

---------
-- Alter
--------
-- Drop distributed key column
create table foo(x int, y int) distributed replicated;
create table bar(like foo) distributed by (x);

insert into foo values(1,1);
insert into bar values(1,1);

-- success
alter table foo drop column x;
-- fail
alter table bar drop column x;

drop table if exists foo;
drop table if exists foo1;
drop table if exists bar;
drop table if exists bar1;

-- Alter gp_distribution_policy
create table foo(x int, y int) distributed replicated;
create table foo1(x int, y int) distributed replicated;
create table bar(x int, y int) distributed by (x);
create table bar1(x int, y int) distributed randomly;

insert into foo select i,i from generate_series(1,10) i;
insert into foo1 select i,i from generate_series(1,10) i;
insert into bar select i,i from generate_series(1,10) i;
insert into bar1 select i,i from generate_series(1,10) i;

-- alter distribution policy of replicated table
alter table foo set distributed by (x);
alter table foo1 set distributed randomly;
-- alter a partitioned table to replicated table
alter table bar set distributed replicated;
alter table bar1 set distributed replicated;

-- verify the new policies
\d foo
\d foo1
\d bar
\d bar1

-- verify the reorganized data
select * from foo;
select * from foo1;
select * from bar;
select * from bar1;

-- alter back
alter table foo set distributed replicated;
alter table foo1 set distributed replicated;
alter table bar set distributed by (x);
alter table bar1 set distributed randomly;

-- verify the policies again
\d foo
\d foo1
\d bar
\d bar1

-- verify the reorganized data again
select * from foo;
select * from foo1;
select * from bar;
select * from bar1;

drop table if exists foo;
drop table if exists foo1;
drop table if exists bar;
drop table if exists bar1;

---------
-- UPDATE / DELETE
---------
create table foo(x int, y int) distributed replicated;
create table bar(x int, y int);
insert into foo values (1, 1), (2, 1);
insert into bar values (1, 2), (2, 2);
update foo set y = 2 where y = 1;
select * from foo;
update foo set y = 1 from bar where bar.y = foo.y;
select * from foo;
delete from foo where y = 1;
select * from foo;

-- Test replicate table within init plan
insert into foo values (1, 1), (2, 1);
select * from bar where exists (select * from foo);

------
-- Test Current Of is disabled for replicated table
------
begin;
declare c1 cursor for select * from foo;
fetch 1 from c1;
delete from foo where current of c1;
abort;

begin;
declare c1 cursor for select * from foo;
fetch 1 from c1;
update foo set y = 1 where current of c1;
abort;

-----
-- Test updatable view works for replicated table
----
truncate foo;
truncate bar;
insert into foo values (1, 1);
insert into foo values (2, 2);
insert into bar values (1, 1);
create view v_foo as select * from foo where y = 1;
begin;
update v_foo set y = 2; 
select * from gp_dist_random('foo');
abort;

update v_foo set y = 3 from bar where bar.y = v_foo.y; 
select * from gp_dist_random('foo');
-- Test gp_segment_id for replicated table
-- gp_segment_id is ambiguous for replicated table, it's been disabled now.
create table baz (c1 int, c2 int) distributed replicated;
create table qux (c1 int, c2 int);

select gp_segment_id from baz;
select xmin from baz;
select xmax from baz;
select ctid from baz;
select * from baz where c2 = gp_segment_id;
select * from baz, qux where baz.c1 = gp_segment_id;
update baz set c2 = gp_segment_id;
update baz set c2 = 1 where gp_segment_id = 1;
update baz set c2 = 1 from qux where gp_segment_id = baz.c1;
insert into baz select i, i from generate_series(1, 1000) i;
vacuum baz;
vacuum full baz;
analyze baz;

-- Test dependencies check when alter table to replicated table
create view v_qux as select ctid from qux;
alter table qux set distributed replicated;
drop view v_qux;
alter table qux set distributed replicated;

-- Test cursor for update also works for replicated table
create table cursor_update (c1 int, c2 int) distributed replicated;
insert into cursor_update select i, i from generate_series(1, 10) i;
begin;
declare c1 cursor for select * from cursor_update order by c2 for update;
fetch next from c1;
end;

-- Test MinMax path on replicated table
create table minmaxtest (x int, y int) distributed replicated;
create index on minmaxtest (x);
insert into minmaxtest select generate_series(1, 10);
set enable_seqscan=off;
select min(x) from minmaxtest;

-- Test replicated on partition table
-- should fail
CREATE TABLE foopart (a int4, b int4) DISTRIBUTED REPLICATED PARTITION BY RANGE (a) (START (1) END (10));
CREATE TABLE foopart (a int4, b int4) PARTITION BY RANGE (a) (START (1) END (10)) ;
-- should fail
ALTER TABLE foopart SET DISTRIBUTED REPLICATED;
ALTER TABLE foopart_1_prt_1 SET DISTRIBUTED REPLICATED;
DROP TABLE foopart;

-- start_ignore
drop schema rpt cascade;
-- end_ignore
