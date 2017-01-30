-- start_ignore
create schema sisc_mat_sort;
set search_path to sisc_mat_sort;
RESET ALL;
-- end_ignore

-- start_ignore
drop function if exists sisc_mat_sort.is_workfile_created(explain_query text);
drop language if exists plpythonu cascade;
create language plpythonu;

-- set workfile is created to true if all segment did it.
create or replace function sisc_mat_sort.is_workfile_created(explain_query text) 
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
language plpythonu;
-- end_ignore

reset statement_mem; 
drop table if exists testsiscm;
create table testsiscm (i1 int, i2 int, i3 int, i4 int); 
insert into testsiscm select i, i % 1000, i % 100000, i % 75 from 
	(select generate_series(1, nsegments * 150000) as i from 
	(select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0) foo) bar;


set statement_mem="3MB";
set gp_resqueue_print_operator_memory_limits=on;
set gp_cte_sharing=on;

set gp_enable_mk_sort=on;
select count(*) from (with ctesisc as 
  (select count(i1) as c1,i3 as c2 from testsiscm group by i3)
select *
from ctesisc as t1, ctesisc as t2
where t1.c1 = t2.c2) foo;
select * from sisc_mat_sort.is_workfile_created('explain analyze
with ctesisc as 
  (select count(i1) as c1,i3 as c2 from testsiscm group by i3)
select *
from ctesisc as t1, ctesisc as t2
where t1.c1 = t2.c2;');
select * from sisc_mat_sort.is_workfile_created('explain analyze
with ctesisc as 
  (select count(i1) as c1,i3 as c2 from testsiscm group by i3)
select *
from ctesisc as t1, ctesisc as t2
where t1.c1 = t2.c2 limit 50000;');

set gp_enable_mk_sort=off;
select count(*) from (with ctesisc as 
  (select count(i1) as c1,i3 as c2 from testsiscm group by i3)
select *
from ctesisc as t1, ctesisc as t2
where t1.c1 = t2.c2) foo;
select * from sisc_mat_sort.is_workfile_created('explain analyze
with ctesisc as 
  (select count(i1) as c1,i3 as c2 from testsiscm group by i3)
select *
from ctesisc as t1, ctesisc as t2
where t1.c1 = t2.c2;');
select * from sisc_mat_sort.is_workfile_created('explain analyze
with ctesisc as 
  (select count(i1) as c1,i3 as c2 from testsiscm group by i3)
select *
from ctesisc as t1, ctesisc as t2
where t1.c1 = t2.c2 limit 50000;');

-- reset guc
reset statement_mem;
reset gp_resqueue_print_operator_memory_limits;
reset gp_cte_sharing;
reset gp_enable_mk_sort;
 	
-- start_ignore
drop schema sisc_mat_sort cascade;
-- end_ignore
RESET ALL;