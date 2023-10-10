-- Test push join below union all feature
--
-- Generation of join below union all alternative can be verified
-- using GUC optimizer_print_xform_results
--
-- This alternative is generated for all queries in this suite, 
-- except for the join of two union all test, and cte test
-- 
-- ORCA's cost model determines whether to choose this alternative
--
-- Intuitively, join below union is desirable when (1) the union all
-- children can benefit from physical join options not available 
-- after the union all operation, such as indexed nested loop join;
-- and (2) the cost of scanning the non-union all side is relatively
-- low, such as a small table size, and existing distribution or
-- duplication  
--
-- This is an ORCA feature. The plan shape is only verified for ORCA
-- plans. Correctness of the plans can be verified by the # of output
-- rows

-- start_ignore
drop schema if exists join_union_all cascade;
-- end_ignore

-- greenplum
create schema join_union_all;
set search_path=join_union_all;
set optimizer_trace_fallback=on;

-- GUC
set optimizer_enable_push_join_below_union_all=on; -- default off

-- distributed, 1 column, 1k rows
create table dist_small_1(c1 int);
insert into dist_small_1 select generate_series(1,1000);

-- distributed, 1 column, 1k rows
create table dist_small_2(c1 int);
insert into dist_small_2 select generate_series(1,1000);

-- distributed, 1 column, 100k rows
create table dist_large_1(c1 int);
insert into dist_large_1 select generate_series(1,100000);

-- distributed, 1 column, 100k rows
create table dist_large_2(c1 int);
insert into dist_large_2 select generate_series(1,100000);

-- distributed, 1 column, 100k rows
create table dist_large_ao(c1 int) with (appendonly=true);
insert into dist_large_ao select generate_series(1,100000);

-- distributed, 1 column, char(4), 1k rows
create table char_small_1(c1 char(4));
insert into char_small_1 select generate_series(1,1000);

-- distributed, 1 column, char(3), 100 rows
create table char_small_2(c1 char(3));
insert into char_small_2 select generate_series(1,100);

-- distributed, 0 rows
-- this is to minimize the cost of scanning inner_1 multiple times,
-- as needed by this test suite to demonstrate the join below union
-- all alternative
create table inner_1(cc int);

-- randomly, 10 rows
create table inner_2(cc int) distributed randomly;
insert into inner_2 select generate_series(1,10);

-- distributed, 0 rows
create table inner_3(cc varchar);

-- partition table, 2 columns, 100k rows, join on partition key
CREATE TABLE part (c1 int, c2 int) partition by list(c2) (
partition part1 VALUES (1, 2, 3, 4), 
partition part2 VALUES (5, 6, 7), 
partition part3 VALUES (8, 9, 0));
INSERT INTO part SELECT i, i%10 FROM generate_series(1, 100000) i;

-- distribution table, 2 columns, 100k rows, join on distribution key
CREATE TABLE dist (c1 int, c2 int) distributed by (c2);
INSERT INTO dist SELECT i, i FROM generate_series(1, 100000) i;

-- randomly distributed table, 2 columns, 100k rows
CREATE TABLE rand (c1 int, c2 int) distributed randomly;
INSERT INTO rand SELECT i, i FROM generate_series(1, 100000) i;

-- built index for dist_small_1 and dist_large_1,
-- but not for dist_small_2 or dist_large_2 (yet)
create index dist_small_1_index on dist_small_1 using btree (c1);
create index dist_large_1_index on dist_large_1 using btree (c1);

-- build index for char_small_1
-- but not for char_small_2
create index char_small_1_index on char_small_1 using btree (c1);

-- build index for dist and rand
-- but not for part
create index dist_index on dist using btree (c2);
create index rand_index on rand using btree (c2);

-- analyze
analyze dist_small_1;
analyze dist_small_2;
analyze dist_large_1;
analyze dist_large_2;
analyze dist_large_ao;
analyze char_small_1;
analyze char_small_2;
analyze inner_1;
analyze inner_2;
analyze inner_3;
analyze part;
analyze dist;
analyze rand;

-- view
create view dist_view_small as
select c1 from dist_small_1 union all
select c1 from dist_small_2;

create view dist_view_large as
select c1 from dist_large_1 union all
select c1 from dist_large_2;

create view dist_view_large_uniq as
select c1 from dist_large_1 union
select c1 from dist_large_2;

create view dist_view_large_filter as
select c1 from dist_large_1 where c1 < 90000 union all
select c1 from dist_large_2;

create view dist_view_large_subquery as
select c1 from dist_large_1 where c1 = (select count() from dist_small_1) union all
select c1 from dist_large_2;

create view dist_view_large_ao as
select c1 from dist_large_1 union all
select c1 from dist_large_ao;

create view dist_view_join as
select dist_small_1.c1 from dist_small_1 join dist_small_2
 on dist_small_1.c1 = dist_small_2.c1 union all
select c1 from dist_large_1; 

create view char_view_small as
select c1 from char_small_1 union all
select c1 from char_small_2;

create view part_dist_rand as
select * from part union all
select * from dist union all
select * from rand;

create view part_dist as
select * from part union all
select * from dist;

create view part_dist_filter as
select * from part where c1 < 100 and c2 in (1, 5, 8) union all
select * from dist where c1 < 90000 and c2 > 90000;

create view part_rand as
select * from part union all
select * from rand;

-- equality join predicate 
-- union all of small tables
-- join below union all alternative generated, but not chosen
-- Intuition: Hash join with small outer child is cheaper than
-- pushing join condition down as the index condition
explain analyze select c1 from dist_view_small join inner_1 on c1 = cc;

-- inequality join predicate 
-- union all of small tables
-- join below union all alternative chosen
-- Intuition: Compared to the query above, hash join is not an option
-- due to the inequality join condition. This time, join is pushed 
-- below union all to leverage indexed nested loop join.
explain analyze select c1 from dist_view_small join inner_1 on c1 < cc;

-- union all of large tables
-- join below union all alternative chosen
-- Intuition: pushing join condition down as the index condition
-- is cheaper than hash join with large outer child.
explain analyze select c1 from dist_view_large join inner_1 on c1 = cc;

-- union all of large tables
-- join below union all alternative generated, but not chosen
-- Intuition: Compared to the query above, join's inner child is larger,
-- which has two implications. One, the cost of indexed nested loop join
-- becomes higher. Two, the cost of scanning the inner side twice is
-- higher. Both factors led ORCA to not push join below union all. 
explain analyze select c1 from dist_view_large join inner_2 on c1 = cc;

-- equality join predicate
-- union all of large tables, one with a filter
-- join below union all alternative generated, but not chosen
explain analyze select c1 from dist_view_large_filter join inner_1 on c1 = cc;

-- inequality join predicate
-- union all of large tables, one with a filter
-- join below union all alternative chosen
-- Intuition: Again, once the hash join option is ruled out by the inequality
-- join condition, join is more likely to be pushed down to take advantage of
-- indexed nested loop join.
explain analyze select c1 from dist_view_large_filter join inner_1 on c1 < cc;

-- equality join predicate
-- union all of large tables, one child's filter is a subquery
-- join below union all alternative generated, but not chosen
explain analyze select c1 from dist_view_large_subquery join inner_1 on c1 = cc;

-- inequality join predicate
-- union all of large tables, one child's filter is a subquery
-- join below union all alternative generated, but not chosen
explain analyze select c1 from dist_view_large_subquery join inner_1 on c1 < cc;

-- union all of large tables, one is append only 
-- join below union all alternative chosen
explain analyze select c1 from dist_view_large_ao join inner_1 on c1 = cc;

-- union all of a join and table
-- join below union all alternative chosen
explain analyze select c1 from dist_view_join join inner_1 on c1 = cc;

-- subquery: union all
-- join below union all alternative chosen
explain analyze select c1 from (select c1 from dist_large_1 union all
select c1 from dist_large_2) as inline join inner_1 on c1 = cc;

-- subquery: aggregation
-- join below union all alternative chosen
explain analyze select c1 from dist_view_large join
 (select distinct cc from inner_1) as inline on c1 = cc;

-- subquery: join, equality predicate
-- join below union all alternative chosen, after join order switch
explain analyze select c1 from dist_view_large join
 (select inner_2.cc from inner_1 join inner_2 on inner_1.cc = inner_2.cc) as inline on c1 = cc;

-- subquery: join, inequality predicate
-- join below union all alternative generated, but not chosen
explain analyze select c1 from dist_view_large join
 (select inner_2.cc from inner_1 join inner_2 on inner_1.cc < inner_2.cc) as inline on c1 = cc;

-- left join: union all of large tables
-- join below union all alternative chosen
explain analyze select c1 from inner_1 left join dist_view_large on c1 = cc;

-- right join: union all of large tables
-- join below union all alternative chosen
explain analyze select c1 from dist_view_large right join inner_1 on c1 = cc;

-- union all joined with union
-- join below union all alternative generated, but not chosen
explain analyze select dist_view_large.c1 from dist_view_large
 join dist_view_large_uniq on dist_view_large.c1 = dist_view_large_uniq.c1;

-- union all joined with union all
-- ORCA_FEATURE_NOT_SUPPORTED: push join below TWO union all
explain analyze select dist_view_small.c1 from dist_view_small
 join dist_view_large on dist_view_small.c1 = dist_view_large.c1;

-- cte: union all of large tables
-- join below union all alternative chosen
explain analyze with cte as (select c1 from dist_large_1 union all
 select c1 from dist_large_2) select c1 from cte join inner_1 on c1 = cc;

-- built index for dist_small_2 and dist_large_2,
-- rerun queries that didn't choose the join below union all alternative
create index dist_small_2_index on dist_small_2 using btree (c1);
create index dist_large_2_index on dist_large_2 using btree (c1);

-- union of small tables
-- join below union all alternative chosen
-- Intuition: Compared to the same query before index was built for
-- dist_small_2, ORCA's cost model chooses to push join below union
-- all because this allows both union all children to benefit from 
-- indexed nested loop join (instead of just one child of the two).
explain analyze select c1 from dist_view_small join inner_1 on c1 = cc;

-- union all of large tables, one with a filter
-- join below union all alternative chosen
-- Intuition: Similarly, compared to the same query before index 
-- was built for dist_large_2, ORCA's cost model chooses to push
-- join below union all because this allows both union all children
-- to benefit from indexed nested loop join.
explain analyze select c1 from dist_view_large_filter join inner_1 on c1 = cc;

-- subquery: aggregation of join, inequality predicate
-- join below union all alternative chosen
-- Intuition: This test is so constructed to have a deep (aggregation of join)
-- yet small (deduplicated) inner child. Making it deep is to verify the inner
-- child gets correctly "cloned" with all the columns correctly remapped when 
-- join is pushed below union all. Making it small is to not induce a high cost
-- of scanning it twice, which is necessary in pushing join below union all.
-- The inequality predicate is to rule out the option of hash join, so that
-- the join is more likely to be pushed down union all to leverage indexed nested
-- loop joins.
explain analyze select c1 from dist_view_large join
 (select distinct inner_2.cc from inner_1 join inner_2 on inner_1.cc = inner_2.cc) as inline on c1 < cc;

-- inequality join predicate 
-- union all of small tables
-- join below union all alternative chosen
-- Intuition: This test is to verify the type cast in the join predicate gets
-- correctly remapped when the join is pushed down union all. 
explain analyze select c1 from char_view_small join inner_3 on c1 < cc;

-- union all of partition, distributed, and randomly distributed tables
-- join below union all alternative generated, but not chosen
explain analyze select c2 from part_dist_rand join inner_1 on c2 = cc;

-- union all of partition and distributed tables
-- join below union all alternative chosen
explain analyze select c2 from part_dist join inner_1 on c2 = cc;

-- union all of partition and distributed tables
-- both union all children have multiple filters
-- join below union all alternative chosen
explain analyze select c2 from part_dist_filter join inner_1 on c2 < cc;

-- union all of partition and randomly distributed tables
-- join below union all alternative chosen
explain analyze select c2 from part_rand join inner_1 on c2 = cc;

-- union all of partition, distributed, and randomly distributed tables
-- join below union all alternative generated, but not chosen
explain analyze select c2 from part_dist_rand join inner_2 on c2 = cc;

-- union all of partition and distributed tables
-- join below union all alternative generated, but not chosen
explain analyze select c2 from part_dist join inner_2 on c2 = cc;

-- union all of partition and randomly distributed tables
-- join below union all alternative generated, but not chosen
explain analyze select c2 from part_rand join inner_2 on c2 = cc;

