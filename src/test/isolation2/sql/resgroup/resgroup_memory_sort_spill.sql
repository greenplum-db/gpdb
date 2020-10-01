-- start_matchsubs
-- m/INSERT \d+/
-- s/INSERT \d+/INSERT/
-- end_matchsubs
create schema sort_spill;
set search_path to sort_spill;

-- start_ignore
create language plpython3u;
-- end_ignore

-- set workfile is created to true if all segment did it.
create or replace function sort_spill.is_workfile_created(explain_query text)
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
        workfile_created = int(m.group(2))
        cur_row = int(workfile_created == nsegments)
        result.append(cur_row)
return result
$$
language plpython3u;


create table testsort (i1 int, i2 int, i3 int, i4 int);
insert into testsort select i, i % 1000, i % 100000, i % 75 from
	(select generate_series(1, nsegments * 50000) as i from
	(select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0) foo) bar;

-- start_ignore
DROP ROLE IF EXISTS role1_memory_test;
DROP RESOURCE GROUP rg1_memory_test;
-- end_ignore
CREATE RESOURCE GROUP rg1_memory_test WITH
(concurrency=2, cpu_rate_limit=10, memory_limit=30, memory_shared_quota=0, memory_spill_ratio=1);
CREATE ROLE role1_memory_test SUPERUSER RESOURCE GROUP rg1_memory_test;
SET ROLE TO role1_memory_test;

set gp_resgroup_print_operator_memory_limits=on;

set gp_enable_mk_sort=on;
select avg(i2) from (select i1,i2 from testsort order by i2) foo;
select * from sort_spill.is_workfile_created('explain analyze select i1,i2 from testsort order by i2;');
select * from sort_spill.is_workfile_created('explain analyze select i1,i2 from testsort order by i2 limit 50000;');

set gp_enable_mk_sort=off;
select avg(i2) from (select i1,i2 from testsort order by i2) foo;
select * from sort_spill.is_workfile_created('explain analyze select i1,i2 from testsort order by i2;');
select * from sort_spill.is_workfile_created('explain analyze select i1,i2 from testsort order by i2 limit 50000;');

drop schema sort_spill cascade;

-- start_ignore
RESET ROLE;
DROP ROLE IF EXISTS role1_memory_test;
DROP RESOURCE GROUP rg1_memory_test;
-- end_ignore
