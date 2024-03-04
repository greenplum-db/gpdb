-- start_matchsubs
--# psql 9 changes now shows username on connection. Ignore the added username.
--m/^You are now connected to database/
--s/ as user ".+"//
-- end_matchsubs

-- Ensure that our expectation of pg_statistic's schema is up-to-date
\d+ pg_statistic

--------------------------------------------------------------------------------
-- Scenario: User table without hll flag
--------------------------------------------------------------------------------

create sequence minirepro_foo_b_seq cache 1;
create table minirepro_foo(a int, b int default nextval('minirepro_foo_b_seq'::regclass), c serial) partition by range(a);
create table minirepro_foo_1 partition of minirepro_foo for values from (1) to (5);
insert into minirepro_foo values(1);
analyze minirepro_foo;

-- Generate minirepro

-- start_ignore
\! echo "select * from minirepro_foo;" > ./data/minirepro_q.sql
\! minirepro regression -q data/minirepro_q.sql -f data/minirepro.sql
-- end_ignore

-- Run minirepro
drop table minirepro_foo; -- this will also delete the pg_statistic tuples for minirepro_foo and minirepro_foo_1
drop sequence minirepro_foo_b_seq;

-- start_ignore
\! psql -Xf data/minirepro.sql regression
-- end_ignore

select
    staattnum,
    stainherit,
    stanullfrac,
    stawidth,
    stadistinct,
    stakind1,
    stakind2,
    stakind3,
    stakind4,
    stakind5,
    staop1,
    staop2,
    staop3,
    staop4,
    staop5,
    stacoll1,
    stacoll2,
    stacoll3,
    stacoll4,
    stacoll5,
    stanumbers1,
    stanumbers2,
    stanumbers3,
    stanumbers4,
    stanumbers5,
    stavalues1,
    stavalues2,
    stavalues3,
    stavalues4,
    stavalues5
from pg_statistic where starelid IN ('minirepro_foo'::regclass, 'minirepro_foo_1'::regclass);

-- Cleanup
drop table minirepro_foo;
drop sequence minirepro_foo_b_seq;

--------------------------------------------------------------------------------
-- Scenario: User table with hll flag
--------------------------------------------------------------------------------

create table minirepro_foo(a int) partition by range(a);
create table minirepro_foo_1 partition of minirepro_foo for values from (1) to (5);
insert into minirepro_foo values(1);
analyze minirepro_foo;

-- Generate minirepro

-- start_ignore
\! echo "select * from minirepro_foo;" > data/minirepro_q.sql
\! minirepro regression -q data/minirepro_q.sql -f data/minirepro.sql --hll
-- end_ignore

-- Run minirepro
drop table minirepro_foo; -- this will also delete the pg_statistic tuples for minirepro_foo and minirepro_foo_1

-- start_ignore
\! psql -Xf data/minirepro.sql regression
-- end_ignore

select
    staattnum,
    stainherit,
    stanullfrac,
    stawidth,
    stadistinct,
    stakind1,
    stakind2,
    stakind3,
    stakind4,
    stakind5,
    staop1,
    staop2,
    staop3,
    staop4,
    staop5,
    stacoll1,
    stacoll2,
    stacoll3,
    stacoll4,
    stacoll5,
    stanumbers1,
    stanumbers2,
    stanumbers3,
    stanumbers4,
    stanumbers5,
    stavalues1,
    stavalues2,
    stavalues3,
    stavalues4,
    stavalues5
from pg_statistic where starelid IN ('minirepro_foo'::regclass, 'minirepro_foo_1'::regclass);

-- Cleanup
drop table minirepro_foo;

--------------------------------------------------------------------------------
-- Scenario: User table with escape-worthy characters in stavaluesN
--------------------------------------------------------------------------------

create table minirepro_foo(a text);
insert into minirepro_foo values('1');
analyze minirepro_foo;
-- arbitrarily populate stats data values having text (with quotes) in a slot
-- (without paying heed to the slot's stakind) to test dumpability
set allow_system_table_mods to on;
update pg_statistic set stavalues3='{"hello", "''world''"}'::text[] where starelid='minirepro_foo'::regclass;

-- Generate minirepro

-- start_ignore
\! echo "select * from minirepro_foo;" > data/minirepro_q.sql
\! minirepro regression -q data/minirepro_q.sql -f data/minirepro.sql
-- end_ignore

-- Run minirepro
drop table minirepro_foo; -- this should also delete the pg_statistic tuple for minirepro_foo

-- start_ignore
\! psql -Xf data/minirepro.sql regression
-- end_ignore

select stavalues3 from pg_statistic where starelid='minirepro_foo'::regclass;

-- Cleanup
drop table minirepro_foo;

--------------------------------------------------------------------------------
-- Ensure that our expectation of pg_statistic_ext and pg_statistic_ext_data schema is up-to-date
--------------------------------------------------------------------------------

\d+ pg_statistic_ext
\d+ pg_statistic_ext_data

--------------------------------------------------------------------------------
-- Scenario: User table with correlated statistics
--------------------------------------------------------------------------------

create table minirepro_foo(a int, b int);

create statistics dep (dependencies) on a, b from minirepro_foo;
create statistics dist (ndistinct) on a, b from minirepro_foo;
create statistics mcv (mcv) on a, b from minirepro_foo;

insert into minirepro_foo select i%100, i%100 from generate_series(1,10000)i;
analyze minirepro_foo;

-- Generate minirepro

-- start_ignore
\! echo "select * from minirepro_foo" > ./data/minirepro_q.sql
\! minirepro regression -q data/minirepro_q.sql -f data/minirepro.sql
-- end_ignore

-- Run minirepro
drop table minirepro_foo; -- this will also delete the tuples from pg_statistic_ext and pg_statistic_ext_data

-- start_ignore
\! psql -Xf data/minirepro.sql regression
-- end_ignore

-- Verify that correlated stats are updated
select count(*)=3 from pg_statistic_ext where stxname in ('dep', 'dist', 'mcv');

select count(*)=3 from pg_statistic_ext pge, pg_statistic_ext_data pgd where pge.oid = pgd.stxoid and pge.stxname in ('dep', 'dist', 'mcv');
select stxname, stxdndistinct, stxddependencies, pg_mcv_list_items(stxdmcv) from pg_statistic_ext pge, pg_statistic_ext_data pgd where pge.oid = pgd.stxoid and pge.stxname in ('dep', 'dist', 'mcv');

-- Cleanup
drop table minirepro_foo;

--------------------------------------------------------------------------------
-- Scenario: User query with multiple tables of correlated statistics
--------------------------------------------------------------------------------

create table minirepro_foo(a int, b int);
create table minirepro_bar(a int, b int);

create statistics dep1 (dependencies) on a, b from minirepro_foo;
create statistics dist1 (ndistinct) on a, b from minirepro_foo;

create statistics dep2 (dependencies) on a, b from minirepro_bar;
create statistics dist2 (ndistinct) on a, b from minirepro_bar;

insert into minirepro_foo select i%100, i%100 from generate_series(1,10000)i;
insert into minirepro_bar select i%100, i%100 from generate_series(1,10000)i;

analyze minirepro_foo;
analyze minirepro_bar;

-- Generate minirepro

-- start_ignore
\! echo "select * from minirepro_foo join minirepro_bar on minirepro_foo.a = minirepro_bar.a " > ./data/minirepro_q.sql
\! minirepro regression -q data/minirepro_q.sql -f data/minirepro.sql
-- end_ignore

-- Run minirepro
-- this will also delete the tuples from pg_statistic_ext and pg_statistic_ext_data
drop table minirepro_foo;
drop table minirepro_bar;

-- start_ignore
\! psql -Xf data/minirepro.sql regression
-- end_ignore

-- Verify that correlated stats are updated
select count(*)=4 from pg_statistic_ext where stxname in ('dep1', 'dep2', 'dist1', 'dist2');

select count(*)=4 from pg_statistic_ext pge, pg_statistic_ext_data pgd where pge.oid = pgd.stxoid and pge.stxname in ('dep1', 'dep2', 'dist1', 'dist2');
select stxname, stxdndistinct, stxddependencies, stxdmcv from pg_statistic_ext pge, pg_statistic_ext_data pgd where pge.oid = pgd.stxoid and pge.stxname in ('dep1', 'dep2', 'dist1', 'dist2');

-- Cleanup
drop table minirepro_foo;
drop table minirepro_bar;

--------------------------------------------------------------------------------
-- Scenario: Test if minirepro captures non-default optimization settings
--------------------------------------------------------------------------------
create table minirepro_foo(a int, b int);
alter database regression set enable_indexscan to off;
alter database regression set optimizer_join_order to greedy;

-- start_ignore
\! echo "select * from minirepro_foo" > ./data/minirepro_q.sql
\! minirepro regression -q data/minirepro_q.sql -f data/minirepro.sql
-- end_ignore

-- Validate if above set gucs are captured in Non-default gucs section
\! grep -E 'Non-default optimization guc settings|enable_indexscan|optimizer_join_order' data/minirepro.sql

alter database regression reset enable_indexscan;
alter database regression reset optimizer_join_order;
-- Cleanup
drop table minirepro_foo;

--------------------------------------------------------------------------------
-- Scenario: Test if minirepro reports appropriate message if no default
--           planner settings are altered
--------------------------------------------------------------------------------
create table minirepro_foo(a int, b int);

-- start_ignore
\! echo "select * from minirepro_foo" > ./data/minirepro_q.sql
\! minirepro regression -q data/minirepro_q.sql -f data/minirepro.sql
-- end_ignore

-- Validate if appropriate message is reported in Non-default gucs section
\! grep -E 'Non-default optimization guc settings|Using all default guc settings' data/minirepro.sql

-- Cleanup
drop table minirepro_foo;
