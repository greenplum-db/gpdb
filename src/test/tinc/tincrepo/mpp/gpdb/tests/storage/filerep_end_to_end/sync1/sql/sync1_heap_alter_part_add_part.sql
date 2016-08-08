-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC1 Heap Part Table 1
--
create table sync1_heap_alter_part_add_part1 (a char, b int, d char,e text) 
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
 insert into sync1_heap_alter_part_add_part1 values ('a',generate_series(1,5),'b','test_1');
--
-- SYNC1 Heap Part Table 2
--
create table sync1_heap_alter_part_add_part2 (a char, b int, d char,e text) 
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
 insert into sync1_heap_alter_part_add_part2 values ('a',generate_series(1,5),'b','test_1');
--
-- SYNC1 Heap Part Table 3
--
create table sync1_heap_alter_part_add_part3 (a char, b int, d char,e text) 
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
 insert into sync1_heap_alter_part_add_part3 values ('a',generate_series(1,5),'b','test_1');
--
-- SYNC1 Heap Part Table 4
--
create table sync1_heap_alter_part_add_part4 (a char, b int, d char,e text) 
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
 insert into sync1_heap_alter_part_add_part4 values ('a',generate_series(1,5),'b','test_1');
--
-- SYNC1 Heap Part Table 5
--
create table sync1_heap_alter_part_add_part5 (a char, b int, d char,e text) 
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
 insert into sync1_heap_alter_part_add_part5 values ('a',generate_series(1,5),'b','test_1');
--
-- SYNC1 Heap Part Table 6
--
create table sync1_heap_alter_part_add_part6 (a char, b int, d char,e text) 
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
 insert into sync1_heap_alter_part_add_part6 values ('a',generate_series(1,5),'b','test_1');
--
-- SYNC1 Heap Part Table 7
--
create table sync1_heap_alter_part_add_part7 (a char, b int, d char,e text) 
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
 insert into sync1_heap_alter_part_add_part7 values ('a',generate_series(1,5),'b','test_1');
--
-- SYNC1 Heap Part Table 8
--
create table sync1_heap_alter_part_add_part8 (a char, b int, d char,e text) 
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
 insert into sync1_heap_alter_part_add_part8 values ('a',generate_series(1,5),'b','test_1');

--
--
-- ALTER TABLE TO ADD PART
--
--
--
-- ALTER SYNC1 Heap Part Add Parition
--
-- Add partition
--
alter table sync1_heap_alter_part_add_part1 add partition p1 end (11);
--
-- Insert few records into the table
--
 insert into sync1_heap_alter_part_add_part1 values ('a',generate_series(1,5),'b','test_1');
--
-- Set subpartition Template
--
alter table sync1_heap_alter_part_add_part1 set subpartition template ();
--
-- Add Partition
--
alter table sync1_heap_alter_part_add_part1 add partition p3 end (13) (subpartition sp3 values ('c'));
--
-- Insert few records into the table
--
 insert into sync1_heap_alter_part_add_part1 values ('a',generate_series(1,5),'b','test_1');
