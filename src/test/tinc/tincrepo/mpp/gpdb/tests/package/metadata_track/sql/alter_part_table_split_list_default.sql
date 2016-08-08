create table mdt_part_tbl_split_list (a text, b text) partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

alter table mdt_part_tbl_split_list split default partition at ('baz') into (partition bing, default partition);

drop table mdt_part_tbl_split_list;
