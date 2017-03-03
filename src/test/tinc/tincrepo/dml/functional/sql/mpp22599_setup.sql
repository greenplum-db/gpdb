--start_ignore
drop table if exists dml_multi_pt_update;
--end_ignore
create table dml_multi_pt_update(a int, b int, c int,d int )
distributed by (a)
partition by range(b)
subpartition by range(c) 
subpartition template (default subpartition subothers,start (1) end(7) every (4) )
(default partition others, start(1) end(5) every(3));
