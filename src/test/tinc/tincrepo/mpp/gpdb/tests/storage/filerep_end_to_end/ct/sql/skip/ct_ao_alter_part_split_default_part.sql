-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CT AO TABLE 1
--
create table ct_ao_alter_part_split_default_part1 (a text, b text)   with ( appendonly='true') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ct_ao_alter_part_split_default_part1 values ('foo','foo');
insert into ct_ao_alter_part_split_default_part1 values ('bar','bar');
insert into ct_ao_alter_part_split_default_part1 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_split_default_part1;



--
-- CT AO TABLE 2
--
create table ct_ao_alter_part_split_default_part2 (a text, b text)   with ( appendonly='true') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ct_ao_alter_part_split_default_part2 values ('foo','foo');
insert into ct_ao_alter_part_split_default_part2 values ('bar','bar');
insert into ct_ao_alter_part_split_default_part2 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_split_default_part2;

--
-- CT AO TABLE 3
--
create table ct_ao_alter_part_split_default_part3 (a text, b text)   with ( appendonly='true') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ct_ao_alter_part_split_default_part3 values ('foo','foo');
insert into ct_ao_alter_part_split_default_part3 values ('bar','bar');
insert into ct_ao_alter_part_split_default_part3 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_split_default_part3;

--
-- CT AO TABLE 4
--
create table ct_ao_alter_part_split_default_part4 (a text, b text)   with ( appendonly='true') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ct_ao_alter_part_split_default_part4 values ('foo','foo');
insert into ct_ao_alter_part_split_default_part4 values ('bar','bar');
insert into ct_ao_alter_part_split_default_part4 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_split_default_part4;

--
-- CT AO TABLE 5
--
create table ct_ao_alter_part_split_default_part5 (a text, b text)   with ( appendonly='true') partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz);

--
-- Insert few records into the table
--

insert into ct_ao_alter_part_split_default_part5 values ('foo','foo');
insert into ct_ao_alter_part_split_default_part5 values ('bar','bar');
insert into ct_ao_alter_part_split_default_part5 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_split_default_part5;


--
-- ALTER SYNC1 AO 
--
-- split default partition
--
alter table sync1_ao_alter_part_split_default_part4 split default partition at ('baz') into (partition bing, default partition);
--
-- Insert few records into the table
--

insert into sync1_ao_alter_part_split_default_part4 values ('foo','foo');
insert into sync1_ao_alter_part_split_default_part4 values ('bar','bar');
insert into sync1_ao_alter_part_split_default_part4 values ('foo','bar');
--
-- select from the Table
--
select count(*) from sync1_ao_alter_part_split_default_part4;

--
-- ALTER CK_SYNC1 AO 
--
-- split default partition
--
alter table ck_sync1_ao_alter_part_split_default_part3 split default partition at ('baz') into (partition bing, default partition);
--
-- Insert few records into the table
--

insert into ck_sync1_ao_alter_part_split_default_part3 values ('foo','foo');
insert into ck_sync1_ao_alter_part_split_default_part3 values ('bar','bar');
insert into ck_sync1_ao_alter_part_split_default_part3 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_split_default_part3;

--
-- ALTER CT AO 
--
-- split default partition
--
alter table ct_ao_alter_part_split_default_part1 split default partition at ('baz') into (partition bing, default partition);
--
-- Insert few records into the table
--

insert into ct_ao_alter_part_split_default_part1 values ('foo','foo');
insert into ct_ao_alter_part_split_default_part1 values ('bar','bar');
insert into ct_ao_alter_part_split_default_part1 values ('foo','bar');
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_split_default_part1;
