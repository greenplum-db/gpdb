create table mdt_part_tbl_split_list (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);

alter table mdt_part_tbl_split_list split partition for(1) at (1,2) into (partition f1a, partition f1b);

drop table mdt_part_tbl_split_list;

