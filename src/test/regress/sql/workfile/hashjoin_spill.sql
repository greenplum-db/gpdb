-- Ignore "workfile compresssion is not supported by this build" (see
-- 'zlib' test):
--
-- start_matchignore
-- m/ERROR:  workfile compresssion is not supported by this build/
-- end_matchignore

create schema hashjoin_spill;
set search_path to hashjoin_spill;

-- start_ignore
create language plpython3u;
select disable_xform('CXformLeftJoin2RightJoin'); -- disable right join in orca to force left join spilling
-- end_ignore

-- set workfile is created to true if all segment did it.
create or replace function hashjoin_spill.is_workfile_created(explain_query text)
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

CREATE TABLE test_hj_spill (i1 int, i2 int, i3 int, i4 int, i5 int, i6 int, i7 int, i8 int);
insert into test_hj_spill SELECT i,i,i%1000,i,i,i,i,i from
	(select generate_series(1, nsegments * 15000) as i from
	(select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0) foo) bar;
SET statement_mem=1024;
set gp_resqueue_print_operator_memory_limits=on;

set gp_workfile_compression = on;
select avg(i3) from (SELECT t1.* FROM test_hj_spill AS t1 RIGHT JOIN test_hj_spill AS t2 ON t1.i1=t2.i2) foo;
select * from hashjoin_spill.is_workfile_created('explain (analyze, verbose) SELECT t1.* FROM test_hj_spill AS t1 RIGHT JOIN test_hj_spill AS t2 ON t1.i1=t2.i2');
select * from hashjoin_spill.is_workfile_created('explain (analyze, verbose) SELECT t1.* FROM test_hj_spill AS t1 RIGHT JOIN test_hj_spill AS t2 ON t1.i1=t2.i2 LIMIT 15000;');

set gp_workfile_compression = off;
select avg(i3) from (SELECT t1.* FROM test_hj_spill AS t1 RIGHT JOIN test_hj_spill AS t2 ON t1.i1=t2.i2) foo;
select * from hashjoin_spill.is_workfile_created('explain (analyze, verbose) SELECT t1.* FROM test_hj_spill AS t1 RIGHT JOIN test_hj_spill AS t2 ON t1.i1=t2.i2');
select * from hashjoin_spill.is_workfile_created('explain (analyze, verbose) SELECT t1.* FROM test_hj_spill AS t1 RIGHT JOIN test_hj_spill AS t2 ON t1.i1=t2.i2 LIMIT 15000;');

-- Test with a larger data set, so that all the operations don't fit in a
-- single compression buffer.
set gp_workfile_compression = on;
select count(1) from generate_series(1, 1000000) t1 left join generate_series(1, 50000) t2 on t1 = t2;
set gp_workfile_compression = off;
select count(1) from generate_series(1, 1000000) t1 left join generate_series(1, 50000) t2 on t1 = t2;

drop schema hashjoin_spill cascade;
