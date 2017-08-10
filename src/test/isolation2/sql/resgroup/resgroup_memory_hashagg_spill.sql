create schema hashagg_spill;
set search_path to hashagg_spill;

-- start_ignore
create language plpythonu;
-- end_ignore

-- set workfile is created to true if all segment did it.
create or replace function hashagg_spill.is_workfile_created(explain_query text)
returns setof int as
$$
import re
query = "select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0;"
rv = plpy.execute(query)
nsegments = int(rv[0]['nsegments'])
rv = plpy.execute(explain_query)
search_text = 'Work_mem used'
result = []
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        p = re.compile('.+\((seg[\d]+).+ Workfile: \(([\d+]) spilling\)')
        m = p.match(cur_line)
        if not m:
            continue
        workfile_created = int(m.group(2))
        cur_row = int(workfile_created == nsegments)
        result.append(cur_row)
return result
$$
language plpythonu;

create table testhagg (i1 int, i2 int, i3 int, i4 int);
insert into testhagg select i,i,i,i from
	(select generate_series(1, nsegments * 17000) as i from
	(select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0) foo) bar;


-- start_ignore
DROP ROLE IF EXISTS role1_memory_test;
DROP RESOURCE GROUP rg1_memory_test;
-- end_ignore
CREATE ROLE role1_memory_test SUPERUSER;
CREATE RESOURCE GROUP rg1_memory_test WITH
(concurrency=2, cpu_rate_limit=10, memory_limit=10, memory_shared_quota=0, memory_spill_ratio=10);
SET ROLE TO role1_memory_test;

set statement_mem="1800";
0: ALTER ROLE role1_memory_test RESOURCE GROUP none;
0: DROP RESOURCE GROUP rg1_memory_test;
0: CREATE RESOURCE GROUP rg1_memory_test WITH (concurrency=2, cpu_rate_limit=10, memory_limit=30, memory_shared_quota=0, memory_spill_ratio=2);
0: ALTER ROLE role1_memory_test RESOURCE GROUP rg1_memory_test;
set gp_resgroup_print_operator_memory_limits=on;

-- the number of rows returned by the query varies depending on the number of segments, so
-- only print the first 10
select * from (select max(i1) from testhagg group by i2) foo order by 1 limit 10;
select * from hashagg_spill.is_workfile_created('explain analyze select max(i1) from testhagg group by i2;');
select * from hashagg_spill.is_workfile_created('explain analyze select max(i1) from testhagg group by i2 limit 45000;');


-- Test HashAgg with increasing amount of overflows

reset all;

-- Returns the number of overflows from EXPLAIN ANALYZE output
create or replace function hashagg_spill.num_hashagg_overflows(explain_query text)
returns setof int as
$$
import re
query = "select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0;"
rv = plpy.execute(query)
rv = plpy.execute(explain_query)
result = []
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    p = re.compile('.+\((seg[\d]+).+ ([\d+]) overflows;')
    m = p.match(cur_line)
    if m:
      overflows = int(m.group(2))
      result.append(overflows)
return result
$$
language plpythonu;

-- Test agg spilling scenarios
drop table if exists aggspill;
create table aggspill (i int, j int, t text) distributed by (i);
insert into aggspill select i, i*2, i::text from generate_series(1, 10000) i;
insert into aggspill select i, i*2, i::text from generate_series(1, 100000) i;
insert into aggspill select i, i*2, i::text from generate_series(1, 1000000) i;

-- No spill with large statement memory
set statement_mem = '125MB';
0: ALTER ROLE role1_memory_test RESOURCE GROUP none;
0: DROP RESOURCE GROUP rg1_memory_test;
0: CREATE RESOURCE GROUP rg1_memory_test WITH (concurrency=1, cpu_rate_limit=10, memory_limit=60, memory_shared_quota=0, memory_spill_ratio=30);
0: ALTER ROLE role1_memory_test RESOURCE GROUP rg1_memory_test;
select count(*) from (select i, count(*) from aggspill group by i,j having count(*) = 1) g;

-- Reduce the statement memory to induce spilling
set statement_mem = '10MB';
0: ALTER ROLE role1_memory_test RESOURCE GROUP none;
0: DROP RESOURCE GROUP rg1_memory_test;
0: CREATE RESOURCE GROUP rg1_memory_test WITH (concurrency=2, cpu_rate_limit=10, memory_limit=30, memory_shared_quota=0, memory_spill_ratio=10);
0: ALTER ROLE role1_memory_test RESOURCE GROUP rg1_memory_test;
select overflows >= 1 from hashagg_spill.num_hashagg_overflows('explain analyze
select count(*) from (select i, count(*) from aggspill group by i,j having count(*) = 2) g') overflows;
select count(*) from (select i, count(*) from aggspill group by i,j having count(*) = 2) g;

-- Reduce the statement memory, nbatches and entrysize even further to cause multiple overflows
set gp_hashagg_default_nbatches = 4;
set statement_mem = '5MB';
0: ALTER ROLE role1_memory_test RESOURCE GROUP none;
0: DROP RESOURCE GROUP rg1_memory_test;
0: CREATE RESOURCE GROUP rg1_memory_test WITH (concurrency=2, cpu_rate_limit=10, memory_limit=30, memory_shared_quota=0, memory_spill_ratio=5);
0: ALTER ROLE role1_memory_test RESOURCE GROUP rg1_memory_test;

select overflows > 1 from hashagg_spill.num_hashagg_overflows('explain analyze
select count(*) from (select i, count(*) from aggspill group by i,j,t having count(*) = 3) g') overflows;

select count(*) from (select i, count(*) from aggspill group by i,j,t having count(*) = 3) g;

drop schema hashagg_spill cascade;
drop table aggspill;

-- start_ignore
RESET ROLE;
DROP ROLE IF EXISTS role1_memory_test;
DROP RESOURCE GROUP rg1_memory_test;
-- end_ignore
