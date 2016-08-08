-- @product_version gpdb: [4.3.0.0-]
--
-- RESYNC UAOCS Part Table 1
--

create table resync_uaocs_alter_part_drop_part1 (aa date, bb date)   with ( appendonly='true', orientation='column') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into resync_uaocs_alter_part_drop_part1 values ('2007-01-01','2008-01-01');
 insert into resync_uaocs_alter_part_drop_part1 values ('2008-01-01','2009-01-01');
 insert into resync_uaocs_alter_part_drop_part1 values ('2009-01-01','2010-01-01');


select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='resync_uaocs_alter_part_drop_part1');
select count(*) AS only_visi_tups_ins  from resync_uaocs_alter_part_drop_part1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from resync_uaocs_alter_part_drop_part1;
set gp_select_invisible = false;
update resync_uaocs_alter_part_drop_part1 set bb  = '2013-02-02'  where aa = '2009-01-01';
select count(*) AS only_visi_tups_upd  from resync_uaocs_alter_part_drop_part1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from resync_uaocs_alter_part_drop_part1;
set gp_select_invisible = false;
delete from resync_uaocs_alter_part_drop_part1  where aa = '2008-01-01' ;
select count(*) AS only_visi_tups  from resync_uaocs_alter_part_drop_part1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from resync_uaocs_alter_part_drop_part1;
set gp_select_invisible = false;
--
-- RESYNC UAOCS Part Table 2
--

create table resync_uaocs_alter_part_drop_part2 (aa date, bb date)   with ( appendonly='true', orientation='column') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into resync_uaocs_alter_part_drop_part2 values ('2007-01-01','2008-01-01');
 insert into resync_uaocs_alter_part_drop_part2 values ('2008-01-01','2009-01-01');
 insert into resync_uaocs_alter_part_drop_part2 values ('2009-01-01','2010-01-01');
--
-- RESYNC UAOCS Part Table 3
--

create table resync_uaocs_alter_part_drop_part3 (aa date, bb date)   with ( appendonly='true', orientation='column') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into resync_uaocs_alter_part_drop_part3 values ('2007-01-01','2008-01-01');
 insert into resync_uaocs_alter_part_drop_part3 values ('2008-01-01','2009-01-01');
 insert into resync_uaocs_alter_part_drop_part3 values ('2009-01-01','2010-01-01');

--
--
--ALTER TABLE TO DROP PARTITION
--
--

--
-- ALTER SYNC1 UAOCS Table DROP PARTITION
--
-- Add partition 
--
alter table sync1_uaocs_alter_part_drop_part6 add partition a2 start ('2007-02-01') end ('2007-03-01');

insert into sync1_uaocs_alter_part_drop_part6  values ('2007-02-10','2011-01-01');
select count(*) AS only_visi_tups_ins  from sync1_uaocs_alter_part_drop_part6 ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from sync1_uaocs_alter_part_drop_part6 ;
set gp_select_invisible = false;
update sync1_uaocs_alter_part_drop_part6  set bb  = '2013-02-02'  where aa = '2007-02-10';
select count(*) AS only_visi_tups_upd  from sync1_uaocs_alter_part_drop_part6 ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sync1_uaocs_alter_part_drop_part6 ;
set gp_select_invisible = false;
delete from sync1_uaocs_alter_part_drop_part6   where aa = '2007-02-10' ;
select count(*) AS only_visi_tups  from sync1_uaocs_alter_part_drop_part6 ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sync1_uaocs_alter_part_drop_part6 ;
set gp_select_invisible = false;
vacuum sync1_uaocs_alter_part_drop_part6;
select count(*) AS only_visi_tups_vacuum  from sync1_uaocs_alter_part_drop_part6;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_vacuum  from sync1_uaocs_alter_part_drop_part6;
set gp_select_invisible = false;
--
-- Drop partition
--
alter table sync1_uaocs_alter_part_drop_part6 DROP partition a2;
--
-- Drop Partition if exists
--
alter table sync1_uaocs_alter_part_drop_part6 DROP partition if exists a2;

--
-- Add default partition
--
alter table sync1_uaocs_alter_part_drop_part6 add default partition default_part;
--
-- Drop Default Partition
--
alter table sync1_uaocs_alter_part_drop_part6 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into sync1_uaocs_alter_part_drop_part6 values ('2007-01-01','2008-01-01');
 insert into sync1_uaocs_alter_part_drop_part6 values ('2008-01-01','2009-01-01');
 insert into sync1_uaocs_alter_part_drop_part6 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from sync1_uaocs_alter_part_drop_part6;


--
-- ALTER CK_SYNC1 UAOCS Table DROP PARTITION
--
-- Add partition 
--
alter table ck_sync1_uaocs_alter_part_drop_part5 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table ck_sync1_uaocs_alter_part_drop_part5 DROP partition a2;
--
-- Drop Partition if exists
--
alter table ck_sync1_uaocs_alter_part_drop_part5 DROP partition if exists a2;

--
-- Add default partition
--
alter table ck_sync1_uaocs_alter_part_drop_part5 add default partition default_part;
--
-- Drop Default Partition
--
alter table ck_sync1_uaocs_alter_part_drop_part5 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into ck_sync1_uaocs_alter_part_drop_part5 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_uaocs_alter_part_drop_part5 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_uaocs_alter_part_drop_part5 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from ck_sync1_uaocs_alter_part_drop_part5;

--
-- ALTER CT UAOCS Table DROP PARTITION
--
-- Add partition 
--
alter table ct_uaocs_alter_part_drop_part3 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table ct_uaocs_alter_part_drop_part3 DROP partition a2;
--
-- Drop Partition if exists
--
alter table ct_uaocs_alter_part_drop_part3 DROP partition if exists a2;

--
-- Add default partition
--
alter table ct_uaocs_alter_part_drop_part3 add default partition default_part;
--
-- Drop Default Partition
--
alter table ct_uaocs_alter_part_drop_part3 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into ct_uaocs_alter_part_drop_part3 values ('2007-01-01','2008-01-01');
 insert into ct_uaocs_alter_part_drop_part3 values ('2008-01-01','2009-01-01');
 insert into ct_uaocs_alter_part_drop_part3 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from ct_uaocs_alter_part_drop_part3;


--
-- ALTER RESYNC UAOCS Table DROP PARTITION
--
-- Add partition 
--
alter table resync_uaocs_alter_part_drop_part1 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table resync_uaocs_alter_part_drop_part1 DROP partition a2;
--
-- Drop Partition if exists
--
alter table resync_uaocs_alter_part_drop_part1 DROP partition if exists a2;

--
-- Add default partition
--
alter table resync_uaocs_alter_part_drop_part1 add default partition default_part;
--
-- Drop Default Partition
--
alter table resync_uaocs_alter_part_drop_part1 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into resync_uaocs_alter_part_drop_part1 values ('2007-01-01','2008-01-01');
 insert into resync_uaocs_alter_part_drop_part1 values ('2008-01-01','2009-01-01');
 insert into resync_uaocs_alter_part_drop_part1 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from resync_uaocs_alter_part_drop_part1;


