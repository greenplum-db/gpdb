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

create table minirepro_foo(a int) partition by range(a);
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
\! psql -f data/minirepro.sql regression

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
\! psql -f data/minirepro.sql regression

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
\! psql -f data/minirepro.sql regression

select stavalues3 from pg_statistic where starelid='minirepro_foo'::regclass;

-- Cleanup
drop table minirepro_foo;

--------------------------------------------------------------------------------
-- Scenario: Catalog table without hll flag
--------------------------------------------------------------------------------

-- Generate minirepro

-- start_ignore
\! echo "select oid from pg_tablespace;" > data/minirepro_q.sql
\! minirepro regression -q data/minirepro_q.sql -f data/minirepro.sql
-- end_ignore

-- Run minirepro
-- Caution: The following operation will remove the pg_statistic tuple
-- corresponding to pg_tablespace before it re-inserts it, which may lead to
-- corrupted stats for pg_tablespace. But, that shouldn't matter too much?
\! psql -f data/minirepro.sql regression

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
from pg_statistic where starelid='pg_tablespace'::regclass;


