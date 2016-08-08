-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC1 HEAP TABLE 1
--
create table sync1_heap_alter_part_split_default_part1 (a text, b text)  partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_split_default_part1 values ('foo','foo');
insert into sync1_heap_alter_part_split_default_part1 values ('bar','bar');
insert into sync1_heap_alter_part_split_default_part1 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_default_part1;



--
-- SYNC1 HEAP TABLE 2
--
create table sync1_heap_alter_part_split_default_part2 (a text, b text)  partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_split_default_part2 values ('foo','foo');
insert into sync1_heap_alter_part_split_default_part2 values ('bar','bar');
insert into sync1_heap_alter_part_split_default_part2 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_default_part2;

--
-- SYNC1 HEAP TABLE 3
--
create table sync1_heap_alter_part_split_default_part3 (a text, b text)  partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_split_default_part3 values ('foo','foo');
insert into sync1_heap_alter_part_split_default_part3 values ('bar','bar');
insert into sync1_heap_alter_part_split_default_part3 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_default_part3;

--
-- SYNC1 HEAP TABLE 4
--
create table sync1_heap_alter_part_split_default_part4 (a text, b text)  partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_split_default_part4 values ('foo','foo');
insert into sync1_heap_alter_part_split_default_part4 values ('bar','bar');
insert into sync1_heap_alter_part_split_default_part4 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_default_part4;

--
-- SYNC1 HEAP TABLE 5
--
create table sync1_heap_alter_part_split_default_part5 (a text, b text)  partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_split_default_part5 values ('foo','foo');
insert into sync1_heap_alter_part_split_default_part5 values ('bar','bar');
insert into sync1_heap_alter_part_split_default_part5 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_default_part5;

--
-- SYNC1 HEAP TABLE 6
--
create table sync1_heap_alter_part_split_default_part6 (a text, b text)  partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_split_default_part6 values ('foo','foo');
insert into sync1_heap_alter_part_split_default_part6 values ('bar','bar');
insert into sync1_heap_alter_part_split_default_part6 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_default_part6;

--
-- SYNC1 HEAP TABLE 7
--
create table sync1_heap_alter_part_split_default_part7 (a text, b text)  partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_split_default_part7 values ('foo','foo');
insert into sync1_heap_alter_part_split_default_part7 values ('bar','bar');
insert into sync1_heap_alter_part_split_default_part7 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_default_part7;

--
-- SYNC1 HEAP TABLE 8
--
create table sync1_heap_alter_part_split_default_part8 (a text, b text)  partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_split_default_part8 values ('foo','foo');
insert into sync1_heap_alter_part_split_default_part8 values ('bar','bar');
insert into sync1_heap_alter_part_split_default_part8 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_default_part8;


--
-- ALTER SYNC1 HEAP 
--
-- split default partition
--
alter table sync1_heap_alter_part_split_default_part1 split default partition at ('baz') into (partition bing, default partition);
--
-- Insert few records into the table
--

insert into sync1_heap_alter_part_split_default_part1 values ('foo','foo');
insert into sync1_heap_alter_part_split_default_part1 values ('bar','bar');
insert into sync1_heap_alter_part_split_default_part1 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_default_part1;
