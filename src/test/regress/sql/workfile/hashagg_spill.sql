-- start_ignore
create schema hashagg_spill;
set search_path to hashagg_spill;
RESET ALL;
-- end_ignore

-- start_ignore
drop function if exists hashagg_spill.is_workfile_created(explain_query text);
drop language if exists plpythonu cascade;
create language plpythonu;

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
        workfile_created = int(m.group(2))        
        cur_row = int(workfile_created == nsegments)
        result.append(cur_row)        
return result
$$
language plpythonu;
-- end_ignore

drop table if exists testhagg; 
create table testhagg (i1 int, i2 int, i3 int, i4 int);
insert into testhagg select i,i,i,i from 
	(select generate_series(1, nsegments * 15000) as i from 
	(select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0) foo) bar; 

set statement_mem="1800";
set gp_resqueue_print_operator_memory_limits=on;

select count(*) from (select max(i1) from testhagg group by i2) foo;
select * from hashagg_spill.is_workfile_created('explain analyze select max(i1) from testhagg group by i2;');
select * from hashagg_spill.is_workfile_created('explain analyze select max(i1) from testhagg group by i2 limit 45000;');

-- reset guc
reset statement_mem;
reset gp_resqueue_print_operator_memory_limits;

-- start_ignore
drop schema hashagg_spill cascade;
-- end_ignore
RESET ALL;