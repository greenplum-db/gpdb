-- Test deparsing of window functions

set client_min_messages = 'WARNING';
drop schema if exists window_views  cascade;

create schema window_views;
set search_path=window_views;

CREATE TABLE testtab (a int4, b int4);

reset client_min_messages;

create view v1 as
  select avg(a) over (partition by b)
  from testtab;
select pg_get_viewdef('v1');

create view v2 as
  select avg(a) over (order by b)
  from testtab;
select pg_get_viewdef('v2');

create view v3 as
  select avg(a) over (partition by a order by b)
  from testtab;
select pg_get_viewdef('v3');

create view v4 as
  select avg(a) over (w)
  from testtab window w as ();
select pg_get_viewdef('v4');

create view v5 as
  select avg(a) over (w order by b)
  from testtab window w as (partition by a);
select pg_get_viewdef('v5');

create view v6 as
  select avg(a) over (w order by a)
  from testtab window w as ();
select pg_get_viewdef('v6');

create view v7 as
  select avg(a) over (w)
  from testtab window w as (order by a);
select pg_get_viewdef('v7');

create view v8 as
  select avg(a) over (w)
  from testtab window w as (order by a rows between unbounded preceding and unbounded following);
select pg_get_viewdef('v8');

create view v9 as
  select avg(a) over (w)
  from testtab window w as (order by a rows between 1 preceding and 1 following);
select pg_get_viewdef('v9');

create view v10 as
  select avg(a) over (w rows between 1 preceding and 1 following)
  from testtab window w as (order by a);
select pg_get_viewdef('v10');

-- Check that COUNT(*) is dumped with the star intact.
create view v_star as
  select count(*) over (partition by a)
  from testtab;
select pg_get_viewdef('v_star');
