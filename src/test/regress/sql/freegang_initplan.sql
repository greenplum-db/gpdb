create extension if not exists gp_inject_fault;

create table t_freegang_initplan(c int);

create or replace function f_freegang_initplan() returns int as
$$
begin
  insert into t_freegang_initplan select * from generate_series(1, 10);
  return 1;
end;
$$
language plpgsql;

select gp_inject_fault('free_gang_initplan', 'reset', 1);
select gp_inject_fault('free_gang_initplan', 'skip', 1);

-- the following query will generate initplan, and initplan should not
-- cleanup gang allocated to parent plan.
create table t_freegang_initplan_test as select f_freegang_initplan();

select gp_wait_until_triggered_fault('free_gang_initplan', 1, 1);

select * from t_freegang_initplan_test;

drop function f_freegang_initplan();
drop table t_freegang_initplan;
drop table t_freegang_initplan_test;
