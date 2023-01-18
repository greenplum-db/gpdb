create table cost_agg_t1(a int, b int, c int);
insert into cost_agg_t1 select i, random() * 99999, i % 2000 from generate_series(1, 1000000) i;
create table cost_agg_t2 as select * from cost_agg_t1 with no data;
insert into cost_agg_t2 select i, random() * 99999, i % 300000 from generate_series(1, 1000000) i;
analyze cost_agg_t1;
analyze cost_agg_t2;

--
-- Test planner's decisions on aggregates when only little memory is available.
--
set statement_mem= '1800 kB';

-- There are only 2000 distinct values of 'c' in the table, which fits
-- comfortably in an in-memory hash table.
explain select avg(b) from cost_agg_t1 group by c;

-- In the other table, there are 300000 distinct values of 'c', which doesn't
-- fit in statement_mem. The planner chooses to do a single-phase agg for this.
--
-- In the single-phase plan, the aggregation is performed after redistrbuting
-- the data, which means that each node only has to process 1/(# of segments)
-- fraction of the data. That fits in memory, whereas an initial stage before
-- redistributing would not. And it would eliminate only a few rows, anyway.
explain select avg(b) from cost_agg_t2 group by c;

-- But if there are a lot more duplicate values, the two-stage plan becomes
-- cheaper again, even though it doesn't git in memory and has to spill.
insert into cost_agg_t2 select i, random() * 99999,1 from generate_series(1, 200000) i;
analyze cost_agg_t2;
explain select avg(b) from cost_agg_t2 group by c;


drop table cost_agg_t1;
drop table cost_agg_t2;
reset statement_mem;

-- The following case is to test GUC gp_eager_two_phase_agg for planner
-- When it is set true, planner will choose two stage agg.
create table t_planner_force_multi_stage(a int, b int) distributed randomly;
analyze t_planner_force_multi_stage;
show gp_eager_two_phase_agg;
-- the GUC gp_eager_two_phase_agg is default false, the table contains no data
-- so one stage agg will win.
explain (costs off) select b, sum(a) from t_planner_force_multi_stage group by b;
set gp_eager_two_phase_agg = on;
-- when forcing two stage, it should generate two stage agg plan.
explain (costs off) select b, sum(a) from t_planner_force_multi_stage group by b;
reset gp_eager_two_phase_agg;
drop table t_planner_force_multi_stage;

-- Test user-defined aggregate marked safe to execute on replicated slices without motion
CREATE AGGREGATE my_unsafe_avg (float8)
(
    sfunc = float8_accum,
    stype = float8[],
    finalfunc = float8_avg,
    initcond = '{0,0,0}'
);
CREATE AGGREGATE my_safe_avg (float8)
(
    sfunc = float8_accum,
    stype = float8[],
    finalfunc = float8_avg,
    initcond = '{0,0,0}',
    repsafe = true
);
CREATE TABLE a_reptable (a int) DISTRIBUTED REPLICATED;
CREATE TABLE b_reptable (b int) DISTRIBUTED REPLICATED;
EXPLAIN INSERT INTO a_reptable(a) SELECT my_unsafe_avg(b) FROM b_reptable;
EXPLAIN INSERT INTO a_reptable(a) SELECT my_safe_avg(b) FROM b_reptable;
