-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC CO TABLE 1
--
create table resync_co_alter_part_split_default_part1 (a text, b text)   with ( appendonly='true', orientation='column') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into resync_co_alter_part_split_default_part1 values ('foo','foo');
insert into resync_co_alter_part_split_default_part1 values ('bar','bar');
insert into resync_co_alter_part_split_default_part1 values ('foo','bar');
--
-- select from the Table
--
select count(*) from resync_co_alter_part_split_default_part1;



--
-- RESYNC CO TABLE 2
--
create table resync_co_alter_part_split_default_part2 (a text, b text)   with ( appendonly='true', orientation='column') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into resync_co_alter_part_split_default_part2 values ('foo','foo');
insert into resync_co_alter_part_split_default_part2 values ('bar','bar');
insert into resync_co_alter_part_split_default_part2 values ('foo','bar');
--
-- select from the Table
--
select count(*) from resync_co_alter_part_split_default_part2;

--
-- RESYNC CO TABLE 3
--
create table resync_co_alter_part_split_default_part3 (a text, b text)   with ( appendonly='true', orientation='column') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into resync_co_alter_part_split_default_part3 values ('foo','foo');
insert into resync_co_alter_part_split_default_part3 values ('bar','bar');
insert into resync_co_alter_part_split_default_part3 values ('foo','bar');
--
-- select from the Table
--
select count(*) from resync_co_alter_part_split_default_part3;




--
-- ALTER SYNC1 CO 
--
-- split default partition
--
alter table sync1_co_alter_part_split_default_part6 split default partition at ('baz') into (partition bing, default partition);
--
-- Insert few records into the table
--

insert into sync1_co_alter_part_split_default_part6 values ('foo','foo');
insert into sync1_co_alter_part_split_default_part6 values ('bar','bar');
insert into sync1_co_alter_part_split_default_part6 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_split_default_part6;

--
-- ALTER CK_SYNC1 CO 
--
-- split default partition
--
alter table ck_sync1_co_alter_part_split_default_part5 split default partition at ('baz') into (partition bing, default partition);
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
-- ALTER CT CO 
--
-- split default partition
--
alter table ct_co_alter_part_split_default_part3 split default partition at ('baz') into (partition bing, default partition);
--
-- Insert few records into the table
--

insert into ct_co_alter_part_split_default_part3 values ('foo','foo');
insert into ct_co_alter_part_split_default_part3 values ('bar','bar');
insert into ct_co_alter_part_split_default_part3 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ct_co_alter_part_split_default_part3;


--
-- ALTER RESYNC CO 
--
-- split default partition
--
alter table resync_co_alter_part_split_default_part1 split default partition at ('baz') into (partition bing, default partition);
--
-- Insert few records into the table
--

insert into resync_co_alter_part_split_default_part1 values ('foo','foo');
insert into resync_co_alter_part_split_default_part1 values ('bar','bar');
insert into resync_co_alter_part_split_default_part1 values ('foo','bar');
--
-- select from the Table
--
select count(*) from resync_co_alter_part_split_default_part1;
