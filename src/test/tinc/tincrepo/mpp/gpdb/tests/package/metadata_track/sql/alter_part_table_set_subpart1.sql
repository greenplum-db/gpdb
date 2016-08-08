create table mdt_part_tbl_subpartition (a char, b int, d char)
partition by range (b)
subpartition by list (d)
subpartition template (
 subpartition sp1 values ('a'),
 subpartition sp2 values ('b'))
(
start (1) end (4) every (2)
);

alter table mdt_part_tbl_subpartition set subpartition template ();

drop table mdt_part_tbl_subpartition;
