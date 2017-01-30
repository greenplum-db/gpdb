-- start_ignore
create schema materialize_spill;
set search_path to materialize_spill;
RESET ALL;
-- end_ignore

-- start_ignore
drop function if exists materialize_spill.is_workfile_created(explain_query text);
drop language if exists plpythonu cascade;
create language plpythonu;

-- set workfile is created to true if all segment did it.
create or replace function materialize_spill.is_workfile_created(explain_query text) 
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

DROP TABLE IF EXISTS test_mat;
CREATE TABLE test_mat (i1 int, i2 int, i3 int, i4 int, i5 int, i6 int, i7 int, i8 int); 
INSERT INTO test_mat SELECT i,i,i,i,i,i,i,i from 
	(select generate_series(1, nsegments * 15000) as i from 
	(select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0) foo) bar; 

SET statement_mem=1024;
set gp_resqueue_print_operator_memory_limits=on;
set enable_hashjoin = false; 
set enable_nestloop = true; 

select count(*) from (SELECT t1.* FROM test_mat as t1, test_mat AS t2
WHERE t1.i1=t2.i2) foo;
select * from materialize_spill.is_workfile_created('explain analyze SELECT t1.* FROM test_mat as t1, test_mat AS t2
WHERE t1.i1=t2.i2;');
select * from materialize_spill.is_workfile_created('explain analyze SELECT t1.* FROM test_mat as t1, test_mat AS t2
WHERE t1.i1=t2.i2 limit 15000;');

-- reset guc
reset statement_mem;
reset gp_resqueue_print_operator_memory_limits;
reset enable_hashjoin; 
reset enable_nestloop; 

-- start_ignore
drop schema materialize_spill cascade;
-- end_ignore
RESET ALL;