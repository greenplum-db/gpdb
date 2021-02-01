create schema sisc_mat_sort;
set search_path to sisc_mat_sort;

-- start_ignore
create language plpython3u;
-- end_ignore

-- set workfile is created to true if all segment did it.
create or replace function sisc_mat_sort.is_workfile_created(explain_query text)
returns setof int as
$$
import re
query = "select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0;"
rv = plpy.execute(query)
nsegments = int(rv[0]['nsegments'])
rv = plpy.execute(explain_query)
search_text = 'spilling'
result = []
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        p = re.compile('.+\((segment \d+).+ Workfile: \((\d+) spilling\)')
        m = p.match(cur_line)
        workfile_created = int(m.group(2))
        cur_row = int(workfile_created == nsegments)
        result.append(cur_row)
return result
$$
language plpython3u;

create table testsiscm (i1 int, i2 int, i3 int, i4 int);
insert into testsiscm select i, i % 1000, i % 100000, i % 75 from
	(select generate_series(1, nsegments * 150000) as i from
	(select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0) foo) bar;
analyze testsiscm;


set statement_mem="3MB";
set gp_resqueue_print_operator_memory_limits=on;
set gp_cte_sharing=on;
-- The expected output is very sensitive to the kind of plan this produces.
-- We're testing the executor, not the planner, so force ORCA off, to get
-- the particular plan
set optimizer=off;

select count(*) from (with ctesisc as 
	(select count(i1) as c1, i2 as c2, i3 as c3 from testsiscm group by i2, i3) 
select * 
from ctesisc as t1, ctesisc as t2 
where t1.c1 = t2.c1 and t1.c3 = t2.c3) foo;
select * from sisc_mat_sort.is_workfile_created('explain (analyze, verbose)
with ctesisc as 
	(select count(i1) as c1, i2 as c2, i3 as c3 from testsiscm group by i2, i3) 
select * 
from ctesisc as t1, ctesisc as t2 
where t1.c1 = t2.c1 and t1.c3 = t2.c3;');
select * from sisc_mat_sort.is_workfile_created('explain (analyze, verbose)
with ctesisc as 
	(select count(i1) as c1, i2 as c2, i3 as c3 from testsiscm group by i2, i3) 
select * 
from ctesisc as t1, ctesisc as t2 
where t1.c1 = t2.c1 and t1.c3 = t2.c3 limit 50000;');

select count(*) from (with ctesisc as 
	(select count(i1) as c1, i2 as c2, i3 as c3 from testsiscm group by i2, i3) 
select * 
from ctesisc as t1, ctesisc as t2 
where t1.c1 = t2.c1 and t1.c3 = t2.c3) foo;
select * from sisc_mat_sort.is_workfile_created('explain (analyze, verbose)
with ctesisc as 
	(select count(i1) as c1, i2 as c2, i3 as c3 from testsiscm group by i2, i3) 
select * 
from ctesisc as t1, ctesisc as t2 
where t1.c1 = t2.c1 and t1.c3 = t2.c3;');
select * from sisc_mat_sort.is_workfile_created('explain (analyze, verbose)
with ctesisc as 
	(select count(i1) as c1, i2 as c2, i3 as c3 from testsiscm group by i2, i3) 
select * 
from ctesisc as t1, ctesisc as t2 
where t1.c1 = t2.c1 and t1.c3 = t2.c3 limit 50000;');

drop schema sisc_mat_sort cascade;
