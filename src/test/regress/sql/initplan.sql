-- test cases for init plan

-- prepare tables
-- start_ignore
drop table if exists issue_12543_foo, issue_12543_bar;
-- end_ignore
create table issue_12543_foo(a int, b int, c int) distributed randomly
partition by range(a)
(
   start (1) end (2) every (1),
   default partition extra
);
create table issue_12543_bar(a int, b int, c int) distributed randomly;
insert into issue_12543_bar values(1,4,3),(2,4,3);
insert into issue_12543_foo values(5,4,3),(7,4,3),(7,5,3),(7,6,4);

-- CASE 1: single initplan that is not re-used
explain (costs off)
select * from issue_12543_bar
where b < (select count(*) from pg_class);
-- should print all tuples in issue_12543_bar
select * from issue_12543_bar
where b < (select count(*) from pg_class);

-- CASE 2: single init-plan that is re-used by several sub-plans
explain (costs off)
select * from issue_12543_foo
where b < (select count(*) from pg_class);
-- should print all tuples in issue_12543_foo
select * from issue_12543_foo
where b < (select count(*) from pg_class);

-- INIT PLAN that has motion(s)
-- CASE 3: single init-plan that is re-used by several sub-plans
-- GPORCA falls back to postgres planner, but keep it here to
-- compare the results
explain (costs off)
select * from issue_12543_foo
where b > (select max(b) from issue_12543_bar);
select * from issue_12543_foo
where b > (select max(b) from issue_12543_bar);

-- CASE 4: two init-plans that are both re-used by several sub-plans
-- GPORCA falls back to postgres planner, but keep it here to
-- compare the results
explain (costs off)
select * from issue_12543_foo
where b > (select max(b) from issue_12543_bar)
   or c = (select max(c) from issue_12543_bar);
select * from issue_12543_foo
where b > (select max(b) from issue_12543_bar)
   or c = (select max(c) from issue_12543_bar);

-- CASE from Github issue 12543
-- GPORCA falls back to postgres planner, but keep it here to
-- compare the results
explain (costs off)
select * from issue_12543_foo foo
where foo.a in
(select b+c from issue_12543_bar bar
  where a > (select max(b) from issue_12543_bar)
)
or
foo.a in (select b+b from issue_12543_bar);

select * from issue_12543_foo foo
where foo.a in
(select b+c from issue_12543_bar bar
  where a > (select max(b) from issue_12543_bar)
)
or
foo.a in (select b+b from issue_12543_bar);

drop table issue_12543_foo, issue_12543_bar;
