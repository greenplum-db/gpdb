-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC2 Heap Part Table 1
--

create table sync2_heap_alter_part_drop_part1 (aa date, bb date)  partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into sync2_heap_alter_part_drop_part1 values ('2007-01-01','2008-01-01');
 insert into sync2_heap_alter_part_drop_part1 values ('2008-01-01','2009-01-01');
 insert into sync2_heap_alter_part_drop_part1 values ('2009-01-01','2010-01-01');

--
-- SYNC2 Heap Part Table 2
--

create table sync2_heap_alter_part_drop_part2 (aa date, bb date)  partition by range (bb) (partition foo start('2008-01-01'));
--
-- Insert few records into the table
--

 insert into sync2_heap_alter_part_drop_part2 values ('2007-01-01','2008-01-01');
 insert into sync2_heap_alter_part_drop_part2 values ('2008-01-01','2009-01-01');
 insert into sync2_heap_alter_part_drop_part2 values ('2009-01-01','2010-01-01');

--
--
--ALTER TABLE TO DROP PARTITION
--
--

--
-- ALTER SYNC1 Heap Table DROP PARTITION
--
-- Add partition 
--
alter table sync1_heap_alter_part_drop_part7 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table sync1_heap_alter_part_drop_part7 DROP partition a2;
--
-- Drop Partition if exists
--
alter table sync1_heap_alter_part_drop_part7 DROP partition if exists a2;

--
-- Add default partition
--
alter table sync1_heap_alter_part_drop_part7 add default partition default_part;
--
-- Drop Default Partition
--
alter table sync1_heap_alter_part_drop_part7 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into sync1_heap_alter_part_drop_part7 values ('2007-01-01','2008-01-01');
 insert into sync1_heap_alter_part_drop_part7 values ('2008-01-01','2009-01-01');
 insert into sync1_heap_alter_part_drop_part7 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_drop_part7;


--
-- ALTER CK_SYNC1 Heap Table DROP PARTITION
--
-- Add partition 
--
alter table ck_sync1_heap_alter_part_drop_part6 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table ck_sync1_heap_alter_part_drop_part6 DROP partition a2;
--
-- Drop Partition if exists
--
alter table ck_sync1_heap_alter_part_drop_part6 DROP partition if exists a2;

--
-- Add default partition
--
alter table ck_sync1_heap_alter_part_drop_part6 add default partition default_part;
--
-- Drop Default Partition
--
alter table ck_sync1_heap_alter_part_drop_part6 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into ck_sync1_heap_alter_part_drop_part6 values ('2007-01-01','2008-01-01');
 insert into ck_sync1_heap_alter_part_drop_part6 values ('2008-01-01','2009-01-01');
 insert into ck_sync1_heap_alter_part_drop_part6 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_drop_part6;


--
-- ALTER CT Heap Table DROP PARTITION
--
-- Add partition 
--
alter table ct_heap_alter_part_drop_part4 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table ct_heap_alter_part_drop_part4 DROP partition a2;
--
-- Drop Partition if exists
--
alter table ct_heap_alter_part_drop_part4 DROP partition if exists a2;

--
-- Add default partition
--
alter table ct_heap_alter_part_drop_part4 add default partition default_part;
--
-- Drop Default Partition
--
alter table ct_heap_alter_part_drop_part4 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into ct_heap_alter_part_drop_part4 values ('2007-01-01','2008-01-01');
 insert into ct_heap_alter_part_drop_part4 values ('2008-01-01','2009-01-01');
 insert into ct_heap_alter_part_drop_part4 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_drop_part4;


--
-- ALTER RESYNC Heap Table DROP PARTITION
--
-- Add partition 
--
alter table resync_heap_alter_part_drop_part2 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table resync_heap_alter_part_drop_part2 DROP partition a2;
--
-- Drop Partition if exists
--
alter table resync_heap_alter_part_drop_part2 DROP partition if exists a2;

--
-- Add default partition
--
alter table resync_heap_alter_part_drop_part2 add default partition default_part;
--
-- Drop Default Partition
--
alter table resync_heap_alter_part_drop_part2 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into resync_heap_alter_part_drop_part2 values ('2007-01-01','2008-01-01');
 insert into resync_heap_alter_part_drop_part2 values ('2008-01-01','2009-01-01');
 insert into resync_heap_alter_part_drop_part2 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from resync_heap_alter_part_drop_part2;
--
-- ALTER SYNC2 Heap Table DROP PARTITION
--
-- Add partition 
--
alter table sync2_heap_alter_part_drop_part1 add partition a2 start ('2007-02-01') end ('2007-03-01');
--
-- Drop partition
--
alter table sync2_heap_alter_part_drop_part1 DROP partition a2;
--
-- Drop Partition if exists
--
alter table sync2_heap_alter_part_drop_part1 DROP partition if exists a2;

--
-- Add default partition
--
alter table sync2_heap_alter_part_drop_part1 add default partition default_part;
--
-- Drop Default Partition
--
alter table sync2_heap_alter_part_drop_part1 DROP default partition if exists;

--
-- Insert few records into the table
--
 insert into sync2_heap_alter_part_drop_part1 values ('2007-01-01','2008-01-01');
 insert into sync2_heap_alter_part_drop_part1 values ('2008-01-01','2009-01-01');
 insert into sync2_heap_alter_part_drop_part1 values ('2009-01-01','2010-01-01');
--
-- select from the Table
--
select count(*) from sync2_heap_alter_part_drop_part1;


