-- @product_version gpdb: [4.3.0.0-]
--
-- CK_SYNC1 UAO Part Table 1
--

create table ck_sync1_uao_alter_part_drop_part1 (aa date, bb date)   with ( appendonly='true') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into ck_sync1_uao_alter_part_drop_part1 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_uao_alter_part_drop_part1 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_uao_alter_part_drop_part1 values ('2009-01-01','2010-01-01');

select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='ck_sync1_uao_alter_part_drop_part1');
select count(*) AS only_visi_tups_ins  from ck_sync1_uao_alter_part_drop_part1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from ck_sync1_uao_alter_part_drop_part1;
set gp_select_invisible = false;
update ck_sync1_uao_alter_part_drop_part1 set bb  = '2013-02-02'  where aa = '2009-01-01';
select count(*) AS only_visi_tups_upd  from ck_sync1_uao_alter_part_drop_part1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_alter_part_drop_part1;
set gp_select_invisible = false;
delete from ck_sync1_uao_alter_part_drop_part1  where aa = '2008-01-01' ;
select count(*) AS only_visi_tups  from ck_sync1_uao_alter_part_drop_part1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from ck_sync1_uao_alter_part_drop_part1;
set gp_select_invisible = false;

--
-- CK_SYNC1 UAO Part Table 2
--

create table ck_sync1_uao_alter_part_drop_part2 (aa date, bb date)   with ( appendonly='true') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into ck_sync1_uao_alter_part_drop_part2 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_uao_alter_part_drop_part2 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_uao_alter_part_drop_part2 values ('2009-01-01','2010-01-01');
--
-- CK_SYNC1 UAO Part Table 3
--

create table ck_sync1_uao_alter_part_drop_part3 (aa date, bb date)   with ( appendonly='true') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into ck_sync1_uao_alter_part_drop_part3 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_uao_alter_part_drop_part3 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_uao_alter_part_drop_part3 values ('2009-01-01','2010-01-01');
--
-- CK_SYNC1 UAO Part Table 4
--

create table ck_sync1_uao_alter_part_drop_part4 (aa date, bb date)   with ( appendonly='true') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into ck_sync1_uao_alter_part_drop_part4 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_uao_alter_part_drop_part4 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_uao_alter_part_drop_part4 values ('2009-01-01','2010-01-01');
--
-- CK_SYNC1 UAO Part Table 5
--

create table ck_sync1_uao_alter_part_drop_part5 (aa date, bb date)   with ( appendonly='true') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into ck_sync1_uao_alter_part_drop_part5 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_uao_alter_part_drop_part5 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_uao_alter_part_drop_part5 values ('2009-01-01','2010-01-01');
--
-- CK_SYNC1 UAO Part Table 6
--

create table ck_sync1_uao_alter_part_drop_part6 (aa date, bb date)   with ( appendonly='true') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into ck_sync1_uao_alter_part_drop_part6 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_uao_alter_part_drop_part6 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_uao_alter_part_drop_part6 values ('2009-01-01','2010-01-01');
--
-- CK_SYNC1 UAO Part Table 7
--

create table ck_sync1_uao_alter_part_drop_part7 (aa date, bb date)   with ( appendonly='true') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into ck_sync1_uao_alter_part_drop_part7 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_uao_alter_part_drop_part7 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_uao_alter_part_drop_part7 values ('2009-01-01','2010-01-01');

--
--
--ALTER TABLE TO DROP PARTITION
--
--

--
-- ALTER SYNC1 UAO Table DROP PARTITION
--
-- Add partition 
--
alter table sync1_uao_alter_part_drop_part2 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table sync1_uao_alter_part_drop_part2 DROP partition a2;
--
-- Drop Partition if exists
--
alter table sync1_uao_alter_part_drop_part2 DROP partition if exists a2;

--
-- Add default partition
--
alter table sync1_uao_alter_part_drop_part2 add default partition default_part;
--
-- Drop Default Partition
--
alter table sync1_uao_alter_part_drop_part2 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into sync1_uao_alter_part_drop_part2 values ('2007-01-01','2008-01-01');
 insert into sync1_uao_alter_part_drop_part2 values ('2008-01-01','2009-01-01');
 insert into sync1_uao_alter_part_drop_part2 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from sync1_uao_alter_part_drop_part2;




--
-- ALTER CK_SYNC1 UAO Table DROP PARTITION
--
-- Add partition 
--
alter table ck_sync1_uao_alter_part_drop_part1 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table ck_sync1_uao_alter_part_drop_part1 DROP partition a2;
--
-- Drop Partition if exists
--
alter table ck_sync1_uao_alter_part_drop_part1 DROP partition if exists a2;

--
-- Add default partition
--
alter table ck_sync1_uao_alter_part_drop_part1 add default partition default_part;
--
-- Drop Default Partition
--
alter table ck_sync1_uao_alter_part_drop_part1 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into ck_sync1_uao_alter_part_drop_part1 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_uao_alter_part_drop_part1 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_uao_alter_part_drop_part1 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from ck_sync1_uao_alter_part_drop_part1;


