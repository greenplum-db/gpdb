CREATE TABLE mdt_part_tbl_partlist (
        unique1         int4,
        unique2         int4) partition by list (unique1)
( partition aa values (1,2,3,4,5),
  partition bb values (6,7,8,9,10),
  partition cc values (11,12,13,14,15),
  default partition default_part );


CREATE TABLE mdt_part_tbl_partlist_A (
        unique1         int4,
        unique2         int4);

alter table mdt_part_tbl_partlist exchange partition aa with table mdt_part_tbl_partlist_A;

drop table mdt_part_tbl_partlist;
drop table mdt_part_tbl_partlist_A;
