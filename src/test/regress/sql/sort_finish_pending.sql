drop table if exists _tmp_table;
create table _tmp_table (i1 int, i2 int, i3 int, i4 int);
insert into _tmp_table select i, i % 100, i % 10000, i % 75 from generate_series(0,99999) i;

-- make sort to spill
set statement_mem="2MB";
set gp_enable_mk_sort=on;
set gp_cte_sharing=on;

\! gpfaultinjector -f execsort_mksort_mergeruns -y reset --seg_dbid 2 | grep ERROR
-- set QueryFinishPending=true in sort mergeruns. This will stop sort and set result_tape to NULL
\! gpfaultinjector -f execsort_mksort_mergeruns -y finish_pending --seg_dbid 2 | grep ERROR

-- return results although sort will be interrupted in one of the segments 
select DISTINCT S from (select row_number() over(partition by i1 order by i2) AS T, count(*) over (partition by i1) AS S from _tmp_table) AS TMP;
\! gpfaultinjector -f execsort_mksort_mergeruns -y status --seg_dbid 2 | grep "fault injection state:"
