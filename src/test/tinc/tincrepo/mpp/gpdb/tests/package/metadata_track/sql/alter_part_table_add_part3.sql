create table mdt_part_tbl_add (aa date, bb date) partition by range (bb)
(partition foo start('2008-01-01'));

alter table mdt_part_tbl_add add partition a1 start ('2007-01-01') end ('2007-02-01');

drop table mdt_part_tbl_add;
