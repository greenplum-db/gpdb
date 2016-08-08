create table mdt_part_tbl_add (a char, b int, d char)
partition by range (b)
subpartition by list (d)
subpartition template (
 subpartition sp1 values ('a'),
 subpartition sp2 values ('b'))
(
start (1) end (10) every (5)
);

alter table mdt_part_tbl_add add partition p1 end (11);

drop table mdt_part_tbl_add;
