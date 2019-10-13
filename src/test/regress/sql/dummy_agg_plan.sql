create table t_dummy_agg_plan1(c1 int, c2 int) distributed by (c1);
create table t_dummy_agg_plan2(c1 int, c2 int) distributed by (c1);

analyze t_dummy_agg_plan1;
analyze t_dummy_agg_plan2;

explain
  select c1, sum(c2) from t_dummy_agg_plan1 where 1 < 0 group by c1
  union all
  select c1, sum(c2) from t_dummy_agg_plan2 group by c1;

explain
  select count(*), 1 from t_dummy_agg_plan1 where 1 < 0
  union all
  select c1, sum(c2) from t_dummy_agg_plan2 group by c1;
