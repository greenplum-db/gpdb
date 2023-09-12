-- This test verifies ORCA plans when one side of join is
-- of universal spec. Historically, we enforce universal
-- to be joined with singleton to avoid duplicates. This is
-- overly conservative. Instead, we should be able to join
-- universal with any deduplicated input, as far as the join
-- doesn't return all records from the universal side.

-- start_matchsubs
-- m/Memory Usage: \d+\w?B/
-- s/Memory Usage: \d+\w?B/Memory Usage: ###B/
-- m/Memory: \d+kB/
-- s/Memory: \d+kB/Memory: ###kB/
-- m/Buckets: \d+/
-- s/Buckets: \d+/Buckets: ###/
-- m/Hash chain length \d+\.\d+ avg, \d+ max/
-- s/Hash chain length \d+\.\d+ avg, \d+ max/Hash chain length ###/
-- m/using \d+ of \d+ buckets/
-- s/using \d+ of \d+ buckets/using ## of ### buckets/
-- m/Extra Text: \(seg\d+\)/
-- s/Extra Text: \(seg\d+\)/Extra Text: \(seg#\)/
-- end_matchsubs

-- start_ignore
drop schema if exists join_universal cascade;
-- end_ignore

-- greenplum
create schema join_universal;
set search_path=join_universal;
set optimizer_trace_fallback=on;

-- distributed
create table dist (c1 int) distributed by (c1);
insert into dist select i from generate_series(1,999) i;

-- randomly distributed
create table rand (c1 int) distributed randomly;
insert into rand select i from generate_series(1,999) i;

-- replicated
create table rep (c1 int) distributed replicated; 
insert into rep select i from generate_series(1,999) i;

-- partitioned
create table part (c1 int, c2 int) partition by list(c2) (
partition part1 values (1, 2, 3, 4), 
partition part2 values (5, 6, 7), 
partition part3 values (8, 9, 0));
insert into part select i, i%10 from generate_series(1, 999) i;

-- const tvf (universal)
-- This tvf is defined as volatile, but since it's not
-- used as a scan operator, it's distribution spec is
-- still universal instead of singleton.
-- We avoid the "immutable" keyword so that the tvf
-- execution doesn't fall back due to lack of support
-- for Query Parameter.
create function const_tvf(a int) returns int as $$ select $1 $$ language sql;

-- unnested array (universal)
create view unnest_arr as (select unnest(string_to_array('-3,-2,-1,0,1,2,3',','))::int c1);

-- generate_series (universal)
create view gen_series as (select generate_series(-10,10) c1);

analyze dist;
analyze rand;
analyze rep;
analyze part;

-- Testing hash join
set optimizer_enable_hashjoin = on;

-- distributed ⋈ universal 
-- We no more enforce the outer side to be a singleton
-- when the inner side is universal. This allows us to
-- hash the much smaller universal table, instead of
-- the much larger distributed table.
explain (analyze, costs off, timing off, summary off) select * from dist join const_tvf(1) ct(c1) on dist.c1 = ct.c1;
explain (analyze, costs off, timing off, summary off) select * from dist join unnest_arr on dist.c1 = unnest_arr.c1;
explain (analyze, costs off, timing off, summary off) select * from dist join gen_series on dist.c1 = gen_series.c1;

-- randomly distributed ⋈ universal 
-- We get the same plans as above, since no motion is
-- needed when joining with a universal table
-- (We don't flag row count diffs in the following tests.
-- This is because the row count of intermediate physical
-- operations are expected to fluctuate in randomly 
-- distributed tables.)
explain (analyze, timing off, summary off) select * from rand join const_tvf(1) ct(c1) on rand.c1 = ct.c1;
explain (analyze, timing off, summary off) select * from rand join unnest_arr on rand.c1 = unnest_arr.c1;
explain (analyze, timing off, summary off) select * from rand join gen_series on rand.c1 = gen_series.c1;

-- replicated ⋈ universal
-- Replicated joined with universal needs to be deduplicated.
-- This is achieved by a one-time segment filter
-- (duplicate-sensitive random motion).
explain (analyze, costs off, timing off, summary off) select * from rep join const_tvf(1) ct(c1) on rep.c1 = ct.c1;
explain (analyze, costs off, timing off, summary off) select * from rep join unnest_arr on rep.c1 = unnest_arr.c1;
explain (analyze, costs off, timing off, summary off) select * from rep join gen_series on rep.c1 = gen_series.c1;

-- partitioned ⋈ universal 
-- We no more enforce the outer side to be a singleton
-- when the inner side is universal. This allows the
-- propagation of the partition selector, and enables DPE. 
explain (analyze, costs off, timing off, summary off) select * from part join const_tvf(1) ct(c1) on part.c2 = ct.c1;
explain (analyze, costs off, timing off, summary off) select * from part join unnest_arr on part.c2 = unnest_arr.c1;
explain (analyze, costs off, timing off, summary off) select * from part join gen_series on part.c2 = gen_series.c1;

-- distributed ⟕ universal 
-- We get the same plans as those of the inner join, 
-- since the outer table is deduplicated.
explain (analyze, costs off, timing off, summary off) select * from dist left join const_tvf(1) ct(c1) on dist.c1 = ct.c1;
explain (analyze, costs off, timing off, summary off) select * from dist left join unnest_arr on dist.c1 = unnest_arr.c1;
explain (analyze, costs off, timing off, summary off) select * from dist left join gen_series on dist.c1 = gen_series.c1;

-- universal ⟕ distributed
-- Since left join returns all the records from the universal
-- side, it needs to be deduplicated. This is achieved by a
-- hash filter (duplicate-sensitive hash motion).
-- (Test of const TVF left join distributed table is flaky
-- and is turned off. ORCA generates two alternatives, left 
-- join and right join, that happen to have the same cost.)  
explain (analyze, costs off, timing off, summary off) select * from unnest_arr left join dist on dist.c1 = unnest_arr.c1;
explain (analyze, costs off, timing off, summary off) select * from gen_series left join dist on dist.c1 = gen_series.c1;

-- universal ▷ distributed
-- Since anti join returns all the records from the universal
-- side where no matches are found in the deduplicated side,
-- it needs to be deduplicated. This is achieved by a hash
-- filter (duplicate-sensitive hash motion).
explain (analyze, costs off, timing off, summary off) select * from const_tvf(1) ct(c1) where not exists (select 1 from dist where dist.c1 = ct.c1);
explain (analyze, costs off, timing off, summary off) select * from unnest_arr where not exists (select 1 from dist where dist.c1 = unnest_arr.c1);
explain (analyze, costs off, timing off, summary off) select * from gen_series where not exists (select 1 from dist where dist.c1 = gen_series.c1);

-- Testing inner nested loop join
set optimizer_enable_hashjoin = off;

-- We no more enforce the inner side to be a singleton
-- when the outer side is universal. It just needs to
-- be non-replicated since inner join is deduplicated. 
explain (analyze, costs off, timing off, summary off) select * from dist join const_tvf(1) ct(c1) on dist.c1 < ct.c1;
explain (analyze, costs off, timing off, summary off) select * from dist join unnest_arr on dist.c1 < unnest_arr.c1;
explain (analyze, costs off, timing off, summary off) select * from dist join gen_series on dist.c1 < gen_series.c1;
