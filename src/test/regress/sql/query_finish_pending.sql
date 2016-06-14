drop table if exists _tmp_table;
create table _tmp_table (i1 int, i2 int, i3 int, i4 int);
insert into _tmp_table select i, i % 100, i % 10000, i % 75 from generate_series(0,99999) i;

-- make sort to spill
set statement_mem="2MB";
set gp_enable_mk_sort=on;
set gp_cte_sharing=on;


--start_ignore
\! gpfaultinjector -f execsort_mksort_mergeruns -y reset --seg_dbid 2
-- set QueryFinishPending=true in sort mergeruns. This will stop sort and set result_tape to NULL
\! gpfaultinjector -f execsort_mksort_mergeruns -y finish_pending --seg_dbid 2
--end_ignore

-- return results although sort will be interrupted in one of the segments 
select DISTINCT S from (select row_number() over(partition by i1 order by i2) AS T, count(*) over (partition by i1) AS S from _tmp_table) AS TMP;

-- test if shared input scan deletes memory correctly when QueryFinishPending and its child has been eagerly freed,
-- where the child is a Sort node
drop table if exists testsisc;
create table testsisc (i1 int, i2 int, i3 int, i4 int); 
insert into testsisc select i, i % 1000, i % 100000, i % 75 from
(select generate_series(1, nsegments * 50000) as i from 
	(select count(*) as nsegments from gp_segment_configuration where role='p' and content >= 0) foo) bar; 

set gp_resqueue_print_operator_memory_limits=on;
set statement_mem='2MB';
set gp_enable_mk_sort=off;

--start_ignore
\! gpfaultinjector -f execshare_input_next -y reset --seg_dbid 2
-- Set QueryFinishPending to true after SharedInputScan has retrieved the first tuple. 
-- This will eagerly free the memory context of shared input scan's child node.  
\! gpfaultinjector -f execshare_input_next -y finish_pending --seg_dbid 2
--end_ignore

select COUNT(i2) over(partition by i1)
from testsisc
LIMIT 2;


-- test if shared input scan deletes memory correctly when QueryFinishPending and its child has been eagerly freed,
-- where the child is a Sort node and sort_mk algorithm is used

set gp_enable_mk_sort=on;

--start_ignore
\! gpfaultinjector -f execshare_input_next -y reset --seg_dbid 2
-- Set QueryFinishPending to true after SharedInputScan has retrieved the first tuple. 
-- This will eagerly free the memory context of shared input scan's child node.  
\! gpfaultinjector -f execshare_input_next -y finish_pending --seg_dbid 2
--end_ignore

select COUNT(i2) over(partition by i1)
from testsisc
LIMIT 2;

reset gp_enable_mk_sort;

-- test if shared input scan deletes memory correctly when QueryFinishPending and its child has been eagerly freed,
-- where the child is a Materialize node
set statement_mem="5MB";
set gp_cte_sharing=on;

--start_ignore
\! gpfaultinjector -f execshare_input_next -y reset --seg_dbid 2
-- Set QueryFinishPending to true after SharedInputScan has retrieved the first tuple. 
-- This will eagerly free the memory context of shared input scan's child node.  
\! gpfaultinjector -f execshare_input_next -y finish_pending --seg_dbid 2
--end_ignore 
 
with ctesisc as (select i1 as c1,i3 as c2 from testsisc) select count(*) > 0 from ctesisc as t1, ctesisc as t2 where t1.c1 = t2.c2;
