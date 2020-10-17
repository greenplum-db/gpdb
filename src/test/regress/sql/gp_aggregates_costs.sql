create table cost_agg_t1(a int, b int, c int);
insert into cost_agg_t1 select i, random() * 99999, i % 2000 from generate_series(1, 1000000) i;
create table cost_agg_t2 as select * from cost_agg_t1 with no data;
insert into cost_agg_t2 select i, random() * 99999, i % 300000 from generate_series(1, 1000000) i;

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
