-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC1 HEAP TABLE 1
--
create table sync1_heap_alter_part_split_partlist1 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_split_partlist1 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_partlist1;


--
-- SYNC1 HEAP TABLE 2
--
create table sync1_heap_alter_part_split_partlist2 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_split_partlist2 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_partlist2;


--
-- SYNC1 HEAP TABLE 3
--
create table sync1_heap_alter_part_split_partlist3 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_split_partlist3 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_partlist3;


--
-- SYNC1 HEAP TABLE 4
--
create table sync1_heap_alter_part_split_partlist4 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_split_partlist4 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_partlist4;


--
-- SYNC1 HEAP TABLE 5
--
create table sync1_heap_alter_part_split_partlist5 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_split_partlist5 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_partlist5;


--
-- SYNC1 HEAP TABLE 6
--
create table sync1_heap_alter_part_split_partlist6 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_split_partlist6 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_partlist6;

--
-- SYNC1 HEAP TABLE 7
--
create table sync1_heap_alter_part_split_partlist7 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_split_partlist7 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_partlist7;

--
-- SYNC1 HEAP TABLE 8
--
create table sync1_heap_alter_part_split_partlist8 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_split_partlist8 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_partlist8;




--
--ALTER SYNC1 HEAP TABLE
--
--
-- split partition
--
alter table sync1_heap_alter_part_split_partlist1 split partition for(1) at (1,2) into (partition f1a, partition f1b);
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_split_partlist1 values (generate_series(1,10));
--
-- Alter the table set distributed by 
--
Alter table sync1_heap_alter_part_split_partlist1  set with ( reorganize='true') distributed randomly;
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_partlist1;


