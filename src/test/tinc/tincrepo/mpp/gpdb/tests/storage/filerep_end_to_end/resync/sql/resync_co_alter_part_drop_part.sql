-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC CO Part Table 1
--

create table resync_co_alter_part_drop_part1 (aa date, bb date)   with ( appendonly='true', orientation='column') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into resync_co_alter_part_drop_part1 values ('2007-01-01','2008-01-01');
 insert into resync_co_alter_part_drop_part1 values ('2008-01-01','2009-01-01');
 insert into resync_co_alter_part_drop_part1 values ('2009-01-01','2010-01-01');

--
-- RESYNC CO Part Table 2
--

create table resync_co_alter_part_drop_part2 (aa date, bb date)   with ( appendonly='true', orientation='column') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into resync_co_alter_part_drop_part2 values ('2007-01-01','2008-01-01');
 insert into resync_co_alter_part_drop_part2 values ('2008-01-01','2009-01-01');
 insert into resync_co_alter_part_drop_part2 values ('2009-01-01','2010-01-01');
--
-- RESYNC CO Part Table 3
--

create table resync_co_alter_part_drop_part3 (aa date, bb date)   with ( appendonly='true', orientation='column') partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into resync_co_alter_part_drop_part3 values ('2007-01-01','2008-01-01');
 insert into resync_co_alter_part_drop_part3 values ('2008-01-01','2009-01-01');
 insert into resync_co_alter_part_drop_part3 values ('2009-01-01','2010-01-01');

--
--
--ALTER TABLE TO DROP PARTITION
--
--

--
-- ALTER SYNC1 CO Table DROP PARTITION
--
-- Add partition 
--
alter table sync1_co_alter_part_drop_part6 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table sync1_co_alter_part_drop_part6 DROP partition a2;
--
-- Drop Partition if exists
--
alter table sync1_co_alter_part_drop_part6 DROP partition if exists a2;

--
-- Add default partition
--
alter table sync1_co_alter_part_drop_part6 add default partition default_part;
--
-- Drop Default Partition
--
alter table sync1_co_alter_part_drop_part6 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into sync1_co_alter_part_drop_part6 values ('2007-01-01','2008-01-01');
 insert into sync1_co_alter_part_drop_part6 values ('2008-01-01','2009-01-01');
 insert into sync1_co_alter_part_drop_part6 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_drop_part6;


--
-- ALTER CK_SYNC1 CO Table DROP PARTITION
--
-- Add partition 
--
alter table ck_sync1_co_alter_part_drop_part5 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table ck_sync1_co_alter_part_drop_part5 DROP partition a2;
--
-- Drop Partition if exists
--
alter table ck_sync1_co_alter_part_drop_part5 DROP partition if exists a2;

--
-- Add default partition
--
alter table ck_sync1_co_alter_part_drop_part5 add default partition default_part;
--
-- Drop Default Partition
--
alter table ck_sync1_co_alter_part_drop_part5 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into ck_sync1_co_alter_part_drop_part5 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_co_alter_part_drop_part5 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_co_alter_part_drop_part5 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_drop_part5;


--
-- ALTER CT CO Table DROP PARTITION
--
-- Add partition 
--
alter table ct_co_alter_part_drop_part3 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table ct_co_alter_part_drop_part3 DROP partition a2;
--
-- Drop Partition if exists
--
alter table ct_co_alter_part_drop_part3 DROP partition if exists a2;

--
-- Add default partition
--
alter table ct_co_alter_part_drop_part3 add default partition default_part;
--
-- Drop Default Partition
--
alter table ct_co_alter_part_drop_part3 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into ct_co_alter_part_drop_part3 values ('2007-01-01','2008-01-01');
 insert into ct_co_alter_part_drop_part3 values ('2008-01-01','2009-01-01');
 insert into ct_co_alter_part_drop_part3 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from ct_co_alter_part_drop_part3;


--
-- ALTER RESYNC CO Table DROP PARTITION
--
-- Add partition 
--
alter table resync_co_alter_part_drop_part1 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table resync_co_alter_part_drop_part1 DROP partition a2;
--
-- Drop Partition if exists
--
alter table resync_co_alter_part_drop_part1 DROP partition if exists a2;

--
-- Add default partition
--
alter table resync_co_alter_part_drop_part1 add default partition default_part;
--
-- Drop Default Partition
--
alter table resync_co_alter_part_drop_part1 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into resync_co_alter_part_drop_part1 values ('2007-01-01','2008-01-01');
 insert into resync_co_alter_part_drop_part1 values ('2008-01-01','2009-01-01');
 insert into resync_co_alter_part_drop_part1 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from resync_co_alter_part_drop_part1;


