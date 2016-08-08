create table mdt_part_tbl (aa date, bb date) partition by range (bb)
(partition foo start('2008-01-01'));

alter table mdt_part_tbl add partition a2 start ('2007-02-01') end ('2007-03-01');
alter table mdt_part_tbl DROP partition a2;

drop table mdt_part_tbl;
