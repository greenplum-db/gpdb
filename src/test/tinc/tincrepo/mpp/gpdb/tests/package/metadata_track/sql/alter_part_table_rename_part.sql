create table mdt_part_tbl_rename (aa date, bb date) partition by range (bb)
(partition foo start('2008-01-01'));

alter table mdt_part_tbl_rename add partition a2 start ('2007-02-01') end ('2007-03-01');
alter table mdt_part_tbl_rename rename partition a2 to aa2;

drop table mdt_part_tbl_rename;
