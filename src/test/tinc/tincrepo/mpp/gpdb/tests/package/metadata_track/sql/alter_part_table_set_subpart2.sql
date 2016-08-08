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
alter table mdt_part_tbl_subpartition add partition p3 end (13) (subpartition sp3 values ('c'));
alter table mdt_part_tbl_subpartition set subpartition template (subpartition sp3 values ('c'));

drop table mdt_part_tbl_subpartition;
