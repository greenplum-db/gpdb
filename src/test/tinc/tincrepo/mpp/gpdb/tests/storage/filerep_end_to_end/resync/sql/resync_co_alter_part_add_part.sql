-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC CO Part Table 1
--
create table resync_co_alter_part_add_part1 (a char, b int, d char,e text) with ( appendonly='true', orientation='column')
partition by range (b)
subpartition by list (d)
subpartition template (
 subpartition sp1 values ('a'),
 subpartition sp2 values ('b'))
(
start (1) end (10) every (5)
);
--f
-- Insert few records into the table
--
 insert into resync_co_alter_part_add_part1 values ('a',generate_series(1,5),'b','test_1');
--
-- RESYNC CO Part Table 2
--
create table resync_co_alter_part_add_part2 (a char, b int, d char,e text) with ( appendonly='true', orientation='column')
partition by range (b)
subpartition by list (d)
subpartition template (
 subpartition sp1 values ('a'),
 subpartition sp2 values ('b'))
(
start (1) end (10) every (5)
);
--
-- Insert few records into the table
--
 insert into resync_co_alter_part_add_part2 values ('a',generate_series(1,5),'b','test_1');
--
-- RESYNC CO Part Table 3
--
create table resync_co_alter_part_add_part3 (a char, b int, d char,e text) with ( appendonly='true', orientation='column')
partition by range (b)
subpartition by list (d)
subpartition template (
 subpartition sp1 values ('a'),
 subpartition sp2 values ('b'))
(
start (1) end (10) every (5)
);
--
-- Insert few records into the table
--
 insert into resync_co_alter_part_add_part3 values ('a',generate_series(1,5),'b','test_1');
--
--
-- ALTER TABLE TO ADD PART
--
--
--
-- ALTER SYNC1 CO Part Add Partition
--
-- Add partition
--
alter table sync1_co_alter_part_add_part6 add partition p1 end (11);
--
-- Insert few records into the table
--
 insert into sync1_co_alter_part_add_part6 values ('a',generate_series(1,5),'b','test_1');
--
-- Set subpartition Template
--
alter table sync1_co_alter_part_add_part6 set subpartition template ();
--
-- Add Partition
--
alter table sync1_co_alter_part_add_part6 add partition p3 end (13) (subpartition sp3 values ('c'));
--
-- Insert few records into the table
--
 insert into sync1_co_alter_part_add_part6 values ('a',generate_series(1,5),'b','test_1');
--
-- ALTER CK_SYNC1 CO Part Add Partition
--
-- Add partition
--
alter table ck_sync1_co_alter_part_add_part5 add partition p1 end (11);
--
-- Insert few records into the table
--
 insert into ck_sync1_co_alter_part_add_part5 values ('a',generate_series(1,5),'b','test_1');
--
-- Set subpartition Template
--
alter table ck_sync1_co_alter_part_add_part5 set subpartition template ();
--
-- Add Partition
--
alter table ck_sync1_co_alter_part_add_part5 add partition p3 end (13) (subpartition sp3 values ('c'));
--
-- Insert few records into the table
--
 insert into ck_sync1_co_alter_part_add_part5 values ('a',generate_series(1,5),'b','test_1');
--
-- ALTER CT CO Part Add Partition
--
-- Add partition
--
alter table ct_co_alter_part_add_part3 add partition p1 end (11);
--
-- Insert few records into the table
--
 insert into ct_co_alter_part_add_part3 values ('a',generate_series(1,5),'b','test_1');
--
-- Set subpartition Template
--
alter table  ct_co_alter_part_add_part3 set subpartition template ();
--
-- Add Partition
--
alter table ct_co_alter_part_add_part3 add partition p3 end (13) (subpartition sp3 values ('c'));
--
-- Insert few records into the table
--
 insert into ct_co_alter_part_add_part3 values ('a',generate_series(1,5),'b','test_1');
--
-- ALTER RESYNC CO Part Add Partition
--
-- Add partition
--
alter table resync_co_alter_part_add_part1 add partition p1 end (11);
--
-- Insert few records into the table
--
 insert into resync_co_alter_part_add_part1 values ('a',generate_series(1,5),'b','test_1');
--
-- Set subpartition Template
--
alter table resync_co_alter_part_add_part1 set subpartition template ();
--
-- Add Partition
--
alter table resync_co_alter_part_add_part1 add partition p3 end (13) (subpartition sp3 values ('c'));
--
-- Insert few records into the table
--
 insert into resync_co_alter_part_add_part1 values ('a',generate_series(1,5),'b','test_1');
