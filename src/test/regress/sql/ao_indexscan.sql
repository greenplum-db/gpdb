-- Test index scans on AO tables.

-- Create a test table
create table aoindexscantab (i int4, j int4) with (appendonly=true, compresstype=zlib);
insert into aoindexscantab select g, g % 10000 from generate_series(1, 100000) g;
create index on aoindexscantab(j);
create index on aoindexscantab(i);

-- Disable ORCA for these tests. ORCA doesn't currently create index scans on AO
-- tables. Enable this once ORCA learns to do that.
set optimizer=off;

-- Force the use of index scans.
set enable_seqscan=off;
set enable_bitmapscan=off;
set enable_indexscan=on;

-- A simple key-value lookup query. Should use an Index scan, because we disabled
-- bitmap scans.
explain (costs off) select i, j from aoindexscantab where i = 90;
select i, j from aoindexscantab where i = 90;

-- This could be an index-only scan, but we don't support index-only scans on
-- AO tables.
explain (costs off) select i from aoindexscantab where i = 90;
select i from aoindexscantab where i = 90;

-- Slightly more realistic cases for index scans on an AO table, where the
-- planner should choose an index scan over a bitmap scan, not because we
-- forced it, but because it's cheaper
set enable_bitmapscan=on;

explain (costs off) select * from aoindexscantab order by i limit 5;
select * from aoindexscantab order by i limit 5;

explain (costs off) select j from aoindexscantab order by j limit 15;
select j from aoindexscantab order by j limit 15;


-- Also try an AOCS table.
create table aocsindexscantab (i int4, j int4) with (appendonly=true, orientation=column, compresstype=zlib);
insert into aocsindexscantab select g, g % 10000 from generate_series(1, 100000) g;
create index on aocsindexscantab(j);
create index on aocsindexscantab(i);

set enable_bitmapscan=off;
explain (costs off) select i, j from aocsindexscantab where i = 90;
select i, j from aocsindexscantab where i = 90;
explain (costs off) select i from aocsindexscantab where i = 90;
select i from aocsindexscantab where i = 90;
