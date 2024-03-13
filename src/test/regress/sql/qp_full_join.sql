-- This test verifies plans involving full joins, which currently have
-- limited ORCA support. The first half of the tests covers full hash
-- join. Specifically, we examine if the motions are correctly enforced,
-- and if the full hash join delivers correct distribution spec. The
-- second half of the tests (currently don't exist) covers full merge
-- join. Specifically, we examine if the plan alternative is correctly
-- generated, given a versatile combination of join conditions.

-- start_ignore
drop schema if exists full_join cascade;
-- end_ignore

-- greenplum
create schema full_join;
set search_path=full_join;
set optimizer_trace_fallback=on;

-- distributed
create table dist (c1 int) distributed by (c1);
insert into dist select i from generate_series(1,999) i;
insert into dist select null from generate_series(1,10);
create table dist2 (c1 int, c2 int) distributed by (c1);
insert into dist2 select i, i+1 from generate_series(100,1099) i;
insert into dist2 select null from generate_series(1,10);

-- randomly distributed
create table rand (c1 int) distributed randomly;
insert into rand select i from generate_series(-100,899) i;
insert into rand select null from generate_series(1,10);

-- replicated
create table rep (c1 int) distributed replicated; 
insert into rep select i from generate_series(-200,799) i;
insert into rep select null from generate_series(1,20);
create table rep2 (c1 int) distributed replicated; 
insert into rep2 select i from generate_series(-300,699) i;
insert into rep2 select null from generate_series(1,20);

-- universal
create view uni as (select generate_series(-10,10) c1);
create view uni2 as (select unnest(string_to_array('-3,-2,-1,0,1,2,3',','))::int c1);

-- partitioned
create table part (c1 int, c2 int) partition by list(c2) (
partition part1 values (1, 2, 3, 4), 
partition part2 values (5, 6, 7), 
partition part3 values (8, 9, 0),
default partition part4);
insert into part select i, i%10 from generate_series(-400, 599) i;
insert into part select null from generate_series(1,20);

analyze dist;
analyze dist2;
analyze rand;
analyze rep;
analyze rep2;
analyze part;

-- Disable merge join and commutative hash join alternatives, for a clear 
-- view of the distribution request and the derived property given two
-- relations with different distribution policies.

select disable_xform('CXformImplementFullOuterMergeJoin');
select disable_xform('CXformFullJoinCommutativity');
select disable_xform('CXformInnerJoinCommutativity');
select disable_xform('CXformLeftJoin2RightJoin');

--------------------------------------------
-- 2-table join: test requested distribution spec
--------------------------------------------

-- Full hash join sends <hash, hash> and <singleton, singleton> requests to 
-- its children. A quick way to tell which alternative is selected is by 
-- examining whether the full hash join occurs on the segments or coordinator.
-- If the join occurs on the segments, it indicates both inputs are made hash 
-- distributed. If the join occurs on the coordinator, it indicates both inputs
-- are rendered available on the coordinator.
-- 
-- The following tests check the correctness of the distribution requests. 
-- Specifically, we ensure necessary motions aren't missing, and the full
-- join doesn't output duplicates. 

-- distributed ⟗  random
-- The right relation is randomly distributed. It is redistributed to be hash
-- distributed. Both relations are deduplicated to begin with, so there's no
-- duplication risk.
explain (costs off, timing off, summary off) select * from dist full join rand on dist.c1 = rand.c1;
select count(*) from dist full join rand on dist.c1 = rand.c1;

-- distributed ⟗  universal
-- The right relation is universal. To avoid duplicates, the left side is
-- gathered onto the coordinator. 
explain (costs off, timing off, summary off) select * from dist full join uni on dist.c1 = uni.c1;
select count(*) from dist full join uni on dist.c1 = uni.c1;

-- universal ⟗  distributed
-- The left relation is universal. To avoid duplicates, a hash filter 
-- (non-physical motion) is applied to the left side.
explain (costs off, timing off, summary off) select * from uni full join dist on uni.c1 = dist.c1;
select count(*) from uni full join dist on uni.c1 = dist.c1;

-- random ⟗  replicated 
-- The right relation is replicated. To avoid duplicates, both sides are 
-- gathered onto the coordinator.
explain (costs off, timing off, summary off) select * from rand full join rep on rand.c1 = rep.c1;
select count(*) from rand full join rep on rand.c1 = rep.c1;

-- replicated ⟗  random 
-- The left relation is replicated. To avoid duplicates, a hash filter 
-- (non-physical motion) is applied to the left side. The right relation 
-- is randomly distributed. It's redistributed to be hash distributed. 
explain (costs off, timing off, summary off) select * from rep full join rand on rep.c1 = rand.c1;
select count(*) from rep full join rand on rep.c1 = rand.c1;

-- replicated ⟗  universal 
-- The left relation is replicated, and the right relation is universal. To 
-- avoid duplicates, the left side is gathered onto the coordinator.
explain (costs off, timing off, summary off) select * from rep full join uni on rep.c1 = uni.c1;
select count(*) from rep full join uni on rep.c1 = uni.c1;

-- universal ⟗  replicated 
-- The left relation is universal, and the right relation is replicated. To
-- avoid duplicates, the right side is gathered onto the coordinator.
explain (costs off, timing off, summary off) select * from uni full join rep on uni.c1 = rep.c1;
select count(*) from uni full join rep on uni.c1 = rep.c1;

------------------------------------------
-- 3-table join: test derived distribution spec
------------------------------------------

-- The 2-table join tests are effective in assessing required distribution 
-- specs. They provide some coverage for testing derived spec. Essentially, 
-- if the derived spec falls under the Non-Singleton category, the join output 
-- will be gathered onto the coordinator, whereas if the derived spec is 
-- already singleton, no gather motion is necessary. However, for a thorough 
-- examination, 3-table join tests can verify if motions other than gather are 
-- properly enforced, especially in scenarios where the join conditions are 
-- null aware.

-- The following views are named after the likely property (so that the cost
-- is low) that they deliver when used in a join operation. 
create view vw_dist as (select dist.c1 as c11, dist2.c1 as c12 from dist full join dist2 on dist.c1 = dist2.c1);
create view vw_uni as (select uni.c1 as c11, uni2.c1 as c12 from uni full join uni2 on uni.c1 = uni2.c1);
create view vw_sin as (select rep.c1 as c11, uni.c1 as c12 from rep full join uni on rep.c1 = uni.c1);
create view vw_rep as (select uni.c1 as c11, rep.c1 as c12 from uni full join rep on uni.c1 = rep.c1);
create view vw_rep2 as (select rep.c1 as c11, rep2.c1 as c12 from rep full join rep2 on rep.c1 = rep2.c1);

-- Full hash join of two distributed relations most likely delivers the
-- combined hash properties from its two children for all non-null tuples. 
-- If the full join is placed on the left side of an outer join, its output 
-- doesn't need redistribution if the outer join condition isn't null aware. 
-- This is because outer joins send hash distribution requests to their left 
-- child without request null colocation. A motion is necessary, however, if 
-- the join condition is INDF (is not distinct from).
--
-- (distributed ⟗  distributed) ⟕ random
explain (costs off, timing off, summary off) select * from vw_dist left join rand on vw_dist.c11 = rand.c1;
explain (costs off, timing off, summary off) select * from vw_dist left join rand on vw_dist.c12 = rand.c1;
explain (costs off, timing off, summary off) select * from vw_dist left join rand on vw_dist.c11 is not distinct from rand.c1;
explain (costs off, timing off, summary off) select * from vw_dist left join rand on vw_dist.c12 is not distinct from rand.c1;
select count(*) from vw_dist left join rand on vw_dist.c11 = rand.c1;
select count(*) from vw_dist left join rand on vw_dist.c12 = rand.c1;
select count(*) from vw_dist left join rand on vw_dist.c11 is not distinct from rand.c1;
select count(*) from vw_dist left join rand on vw_dist.c12 is not distinct from rand.c1;

-- (distributed ⟗  distributed) ⟗  random
explain (costs off, timing off, summary off) select * from vw_dist full join rand on vw_dist.c11 = rand.c1;
explain (costs off, timing off, summary off) select * from vw_dist full join rand on vw_dist.c12 = rand.c1;
explain (costs off, timing off, summary off) select * from vw_dist full join rand on vw_dist.c11 is not distinct from rand.c1;
explain (costs off, timing off, summary off) select * from vw_dist full join rand on vw_dist.c12 is not distinct from rand.c1;

select count(*) from vw_dist full join rand on vw_dist.c11 = rand.c1;
select count(*) from vw_dist full join rand on vw_dist.c12 = rand.c1;
select count(*) from vw_dist full join rand on vw_dist.c11 is not distinct from rand.c1;
select count(*) from vw_dist full join rand on vw_dist.c12 is not distinct from rand.c1;

-- If the full join is placed on the right side of an outer join, or, is
-- used in an inner join, its output needs redistribution regardless of 
-- the null awareness of the join condition. This is because outer joins 
-- send hash distribution requests to their right child requesting null 
-- colocation, so does inner joins to both children. Note those motions
-- aren't necessary, because null colocation is only relevant if the join
-- condition is null aware. Currently, however, the motion exists because
-- we request tighter specs than that we need. We should look into relaxing 
-- such requests in future.
--
-- random ⟕  (distributed ⟗  distributed)
explain (costs off, timing off, summary off) select * from rand left join vw_dist on rand.c1 = vw_dist.c11;
explain (costs off, timing off, summary off) select * from rand left join vw_dist on rand.c1 = vw_dist.c12;
select count(*) from rand left join vw_dist on rand.c1 = vw_dist.c11;
select count(*) from rand left join vw_dist on rand.c1 = vw_dist.c12;

-- random ⟗  (distributed ⟗  distributed)
explain (costs off, timing off, summary off) select * from rand full join vw_dist on rand.c1 = vw_dist.c11;
explain (costs off, timing off, summary off) select * from rand full join vw_dist on rand.c1 = vw_dist.c12;
select count(*) from rand full join vw_dist on rand.c1 = vw_dist.c11;
select count(*) from rand full join vw_dist on rand.c1 = vw_dist.c12;

-- (distributed ⟗  distributed) ⋈ random
explain (costs off, timing off, summary off) select * from vw_dist join rand on vw_dist.c11 = rand.c1;
explain (costs off, timing off, summary off) select * from vw_dist join rand on vw_dist.c12 = rand.c1;
select count(*) from vw_dist join rand on vw_dist.c11 = rand.c1;
select count(*) from vw_dist join rand on vw_dist.c12 = rand.c1;

-- Full hash join of two universal relations most likely delivers universal 
-- property. In the following tests, the full join is placed on the outer 
-- side of an inner join. To avoid duplicates, a hash filter (non-physical 
-- motion) is applied to that side.
--
-- (universal ⟗  universal) ⋈ random
explain (costs off, timing off, summary off) select * from vw_uni join rand on vw_uni.c11 = rand.c1;
explain (costs off, timing off, summary off) select * from vw_uni join rand on vw_uni.c12 = rand.c1;
select count(*) from vw_uni join rand on vw_uni.c11 = rand.c1;
select count(*) from vw_uni join rand on vw_uni.c12 = rand.c1;

-- Full hash join of a replicated and a universal relation delivers singleton 
-- property. In the following tests, the full join is placed on the outer side
-- of an inner join. Its output data is hash or randomly redistributed across 
-- the segments to maximize MPP execution.
--
-- (replicated ⟗  universal) ⋈ random
explain (costs off, timing off, summary off) select * from vw_sin join rand on vw_sin.c11 = rand.c1;
explain (costs off, timing off, summary off) select * from vw_sin join rand on vw_sin.c12 = rand.c1;
select count(*) from vw_sin join rand on vw_sin.c11 = rand.c1;
select count(*) from vw_sin join rand on vw_sin.c12 = rand.c1;

-- Full hash join of a universal and a replicated relation most likely 
-- delivers replicated property. In the following tests, the full join is 
-- placed on the inner side of an inner join. With the inner side being 
-- replicated, the inner join executes without motion.
--
-- random ⋈ (universal ⟗  replicated)
explain (costs off, timing off, summary off) select * from rand join vw_rep on rand.c1 = vw_rep.c11;
explain (costs off, timing off, summary off) select * from rand join vw_rep on rand.c1 = vw_rep.c12;
select count(*) from rand join vw_rep on rand.c1 = vw_rep.c11;
select count(*) from rand join vw_rep on rand.c1 = vw_rep.c12;

-- Full hash join of two replicated relations most likely delivers replicated 
-- property. In the following tests, the full join is placed on the inner side 
-- of an inner join. With the inner side being replicated, the inner join 
-- executes without motion.
--
-- random ⋈ (replicated ⟗  replicated)
explain (costs off, timing off, summary off) select * from rand join vw_rep2 on rand.c1 = vw_rep2.c11;
explain (costs off, timing off, summary off) select * from rand join vw_rep2 on rand.c1 = vw_rep2.c12;
select count(*) from rand join vw_rep2 on rand.c1 = vw_rep2.c11;
select count(*) from rand join vw_rep2 on rand.c1 = vw_rep2.c12;

--------------------------------------------
-- Self join: test derived distribution spec
--------------------------------------------

-- Spec derivation of self full join generally follows that of 2-table
-- full join. One exception is when a distributed table full joins
-- itself on the same column. This guarantees match in each non-null tuple, 
-- ensuring nulls stay colocated.

create view vw_self_same_col as (select o1.c1 as c11, o2.c1 as c12 from dist2 o1 full join (select distinct c1 from dist2) o2 on o1.c1 = o2.c1);
create view vw_self_diff_col as (select o1.c1 as c11, o1.c2 as c12, o2.c1 as c21, o2.c2 as c22 from dist2 o1 full join dist2 o2 on o1.c1 = o2.c2);
create view vw_self_expr_col as (select o1.c1 as c11, o2.c1 as c12 from dist2 o1 full join (select distinct c1 from dist2) o2 on o1.c1 = o2.c1+1);

-- (distributed ⟗  itself on the same column) ⋈ distributed
-- Self full hash join of a distributed relation on the same column delivers 
-- the combined hash properties from its two children, with null tuples 
-- colocated. Therefore, its output doesn't need to be redistributed, even 
-- when the join condition is null aware. 
explain (costs off, timing off, summary off) select * from vw_self_same_col join dist on vw_self_same_col.c11 is not distinct from dist.c1;
select count(*) from vw_self_same_col join dist on vw_self_same_col.c11 is not distinct from dist.c1;

-- (distributed ⟗  itself on different columns) ⋈ distributed
-- Self full hash join of a distributed relation on different columns, on the,
-- other hand, doesn't ensure null colocation. Its output does require 
-- redistribution to colocate null tuples if the join condition is null aware. 
explain (costs off, timing off, summary off) select * from vw_self_diff_col join dist on vw_self_diff_col.c11 is not distinct from dist.c1;
select count(*) from vw_self_diff_col join dist on vw_self_diff_col.c11 is not distinct from dist.c1;

-- (distributed ⟗  itself on the same column with an expression) ⋈ distributed
-- Self full hash join of a distributed relation on the same column, with
-- an expression applied to one side, is essentially a join on different
-- columns. Its output needs to be redistributed to colocate null tuples if 
-- the join condition is null aware. 
explain (costs off, timing off, summary off) select * from vw_self_expr_col join dist on vw_self_expr_col.c11 is not distinct from dist.c1;
select count(*) from vw_self_expr_col join dist on vw_self_expr_col.c11 is not distinct from dist.c1;

--------------------------------
-- dynamic partition elimination
--------------------------------

-- Similar to left join, DPE is disabled for full join. This is because for 
-- each right tuple scanned, we cannot skip scanning partitions from the left 
-- side. In outer join, the left table is (also) an outer table, where all the 
-- tuples are output with or without a match.
--
-- partitioned ⟗  replicated
explain (costs off, timing off, summary off) select * from part full join rep on part.c1 = rep.c1 and part.c2 = rep.c1;
select count(*) from part full join rep on part.c1 = rep.c1 and part.c2 = rep.c1;

-- (We don't test static partition elimination here because full join with a
-- null filtering predicate that can be used for SPE is converted to left join
-- in expression normalizer. It doesn't enter the logic of full hash join.)

------------------------
-- commutative transform
------------------------

-- distributed ⟗  distributed
-- Full hash join commutative transform adds plan alternative where left and 
-- right children swap their places. Cost model chooses the more performant
-- alternative where the smaller relation is placed on the right side, i.e., 
-- hash side. Here dist is the smaller relation, with a tuple count 1 order
-- of magnitude lower than that of relation dist2.
explain (costs off, timing off, summary off) select * from dist full join dist2 on dist.c1 = dist2.c1;
select enable_xform('CXformFullJoinCommutativity');
explain (costs off, timing off, summary off) select * from dist full join dist2 on dist.c1 = dist2.c1;
