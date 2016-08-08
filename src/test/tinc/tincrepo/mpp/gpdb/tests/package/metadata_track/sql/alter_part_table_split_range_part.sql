create table mdt_part_tbl_split_range (i int) partition by range(i) (start(1) end(10) every(5));

alter table mdt_part_tbl_split_range split partition for(1) at (5) into (partition aa, partition bb);

drop table mdt_part_tbl_split_range;
