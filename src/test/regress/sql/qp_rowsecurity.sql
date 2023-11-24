create user new_user;
create table foo ( a int, b int, c int) distributed by (a);
create table bar ( p int, q int, r int) distributed by (p);
grant select on foo to new_user;
grant select on bar to new_user;
alter table foo enable row level security;
alter table bar enable row level security;

-- Permissive policy
create policy p1 on foo as permissive for select using (foo.a>7);
set session authorization new_user;
explain (costs off ) select * from foo;
reset session authorization;
drop policy p1 on foo;

-- All permissive policies which are applicable to a given query
-- will be combined together using the Boolean “OR” operator
create policy p1 on foo as permissive for select using (foo.a=4);
create policy p2 on foo as permissive for select using (foo.b=60);
create policy p3 on foo as permissive for select using (foo.c=800);
set session authorization new_user ;
explain (costs off )select * from foo;
reset session authorization;
drop policy p1 on foo;
drop policy p2 on foo;
drop policy p3 on foo;

-- Restrictive policies
-- There needs to be at least one permissive policy to grant access to records
create policy p1 on foo as restrictive for select using (foo.a>4);
create policy p2 on foo as restrictive for select using (foo.a<>7);
set session authorization new_user;
explain (costs off ) select * from foo;
reset session authorization ;
-- All restrictive policies which are applicable to a given query will be
-- combined together using the Boolean “AND” operator
create policy p3 on foo as permissive for select using (foo.b>50);
create policy p4 on foo as permissive for select using (foo.c>200);
set session authorization new_user;
explain (costs off ) select * from foo;
reset session authorization;
drop policy p1 on foo;
drop policy p2 on foo;
drop policy p3 on foo;
drop policy p4 on foo;

-- Security Quals should be executed before any other quals
create policy p1 on foo as permissive for select using (foo.a = foo.b);
set session authorization new_user;
explain (costs off ) select * from foo where foo.b=20;
reset session authorization;
drop policy p1 on foo;
create policy p1 on foo as permissive for select using (foo.a=foo.c);
create policy p2 on bar as permissive for select using (bar.p=bar.r);
set session authorization new_user;
explain (costs off) select * from foo join bar on foo.b=bar.q where foo.a=6 and bar.r>=10;
reset session authorization;
drop policy p1 on foo;
drop policy p2 on bar;
create policy p1 on foo as permissive for select using (foo.a > LEAST (foo.a,foo.b));
set session authorization new_user;
explain (costs off) with cte as (select * from foo where foo.b <>10) select * from cte where cte.c>30;
reset session authorization;
drop policy p1 on foo;
drop table foo;
drop table bar;

-- If there is a qual like
-- {Qual=[{Bool_And,a>=20,UDF(b)}]} : Qual is a list with a single clause
-- which is a Bool AND expression
-- If a>=20 evaluates to NULL then the second arg UDF(b) needs to be evaluated
-- which can cause data leak. So need to remove the explicit AND
create table foo ( a int, b int) distributed by (a);
grant select on foo to new_user;
insert into foo (a,b) values(NULL,1),(NULL,2);
insert into foo select i,i from generate_series(20,25)i;
alter table foo enable row level security;
create policy p1 on foo as permissive for select using (foo.a >= 20);
set session authorization new_user;
CREATE OR REPLACE FUNCTION f_leak(int) RETURNS boolean AS $$
BEGIN
  RAISE NOTICE 'Value of column is: %',$1;
RETURN true;
END;
$$ COST 1 LANGUAGE plpgsql;
explain (costs off) select * from foo where f_leak(b);
select * from foo where f_leak(b);
drop function f_leak(int);
reset session authorization;
drop policy p1 on foo;

-- ORCA will fallback to planner for below cases
create table bar ( p int, q int) distributed by (p);
grant select on bar to new_user;

-- Case 1: Sublink present in the query
create policy p1 on foo as permissive for select using (foo.a in (select p from bar));
set session authorization new_user;
set optimizer_trace_fallback to on;
explain (costs off) select * from foo;
reset session authorization ;
drop policy p1 on foo;
drop table bar;

-- Case 2: While generating Index plans
create index btindex on foo using btree(b);
create policy p1 on foo as permissive for select using (foo.a = 20);
set session authorization new_user;
set enable_seqscan to off;
explain (costs off) select * from foo where foo.b=10;
reset enable_seqscan;
reset optimizer_trace_fallback;
reset session authorization ;

drop policy p1 on foo;
drop index btindex;
drop table foo;
drop user new_user;
