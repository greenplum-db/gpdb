-- @optimizer_mode both

-- Make sure random default distribution works for CTAS
select attrnums from gp_distribution_policy where
  localoid = 'distpol'::regclass;

-- Test RANDOM default distribution with AS clause containing a SELECT block
select attrnums from gp_distribution_policy where localoid='bar'::regclass;

-- Test RANDOM distribution with ON COMMIT option
begin;

drop table if exists r3_1;
create temp table r3_1 on commit preserve rows as select 10 as a1, 20 as b1, 30 as c1, 40 as d1;
select attrnums from gp_distribution_policy where localoid='r3_1'::regclass;

drop table if exists r3_2;
create temp table r3_2 on commit delete rows as select 10 as a2, 20 as b2, 30 as c2, 40 as d2;
select attrnums from gp_distribution_policy where localoid='r3_2'::regclass;

drop table if exists r3_3;
create temp table r3_3 on commit drop as select 10 as a3, 20 as b3, 30 as c3, 40 as d3;
select attrnums from gp_distribution_policy where localoid='r3_3'::regclass;

end;
