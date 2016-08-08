-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CK_SYNC1 CO TABLE 1
--
create table ck_sync1_co_alter_part_split_default_part1 (a text, b text)   with ( appendonly='true', orientation='column') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_split_default_part1 values ('foo','foo');
insert into ck_sync1_co_alter_part_split_default_part1 values ('bar','bar');
insert into ck_sync1_co_alter_part_split_default_part1 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_split_default_part1;



--
-- CK_SYNC1 CO TABLE 2
--
create table ck_sync1_co_alter_part_split_default_part2 (a text, b text)   with ( appendonly='true', orientation='column') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_split_default_part2 values ('foo','foo');
insert into ck_sync1_co_alter_part_split_default_part2 values ('bar','bar');
insert into ck_sync1_co_alter_part_split_default_part2 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_split_default_part2;

--
-- CK_SYNC1 CO TABLE 3
--
create table ck_sync1_co_alter_part_split_default_part3 (a text, b text)   with ( appendonly='true', orientation='column') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_split_default_part3 values ('foo','foo');
insert into ck_sync1_co_alter_part_split_default_part3 values ('bar','bar');
insert into ck_sync1_co_alter_part_split_default_part3 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_split_default_part3;

--
-- CK_SYNC1 CO TABLE 4
--
create table ck_sync1_co_alter_part_split_default_part4 (a text, b text)   with ( appendonly='true', orientation='column') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_split_default_part4 values ('foo','foo');
insert into ck_sync1_co_alter_part_split_default_part4 values ('bar','bar');
insert into ck_sync1_co_alter_part_split_default_part4 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_split_default_part4;

--
-- CK_SYNC1 CO TABLE 5
--
create table ck_sync1_co_alter_part_split_default_part5 (a text, b text)   with ( appendonly='true', orientation='column') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_split_default_part5 values ('foo','foo');
insert into ck_sync1_co_alter_part_split_default_part5 values ('bar','bar');
insert into ck_sync1_co_alter_part_split_default_part5 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_split_default_part5;

--
-- CK_SYNC1 CO TABLE 6
--
create table ck_sync1_co_alter_part_split_default_part6 (a text, b text)   with ( appendonly='true', orientation='column') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_split_default_part6 values ('foo','foo');
insert into ck_sync1_co_alter_part_split_default_part6 values ('bar','bar');
insert into ck_sync1_co_alter_part_split_default_part6 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_split_default_part6;

--
-- CK_SYNC1 CO TABLE 7
--
create table ck_sync1_co_alter_part_split_default_part7 (a text, b text)   with ( appendonly='true', orientation='column') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_split_default_part7 values ('foo','foo');
insert into ck_sync1_co_alter_part_split_default_part7 values ('bar','bar');
insert into ck_sync1_co_alter_part_split_default_part7 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_split_default_part7;

--
-- ALTER SYNC1 CO 
--
-- split default partition
--
alter table sync1_co_alter_part_split_default_part2 split default partition at ('baz') into (partition bing, default partition);
--
-- Insert few records into the table
--

insert into sync1_co_alter_part_split_default_part2 values ('foo','foo');
insert into sync1_co_alter_part_split_default_part2 values ('bar','bar');
insert into sync1_co_alter_part_split_default_part2 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_split_default_part2;


--
-- ALTER CK_SYNC1 CO 
--
-- split default partition
--
alter table ck_sync1_co_alter_part_split_default_part1 split default partition at ('baz') into (partition bing, default partition);
--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_split_default_part1 values ('foo','foo');
insert into ck_sync1_co_alter_part_split_default_part1 values ('bar','bar');
insert into ck_sync1_co_alter_part_split_default_part1 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_split_default_part1;
