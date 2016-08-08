-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC CO TABLE 1
--
create table resync_co_alter_part_split_partlist1 (i int)  with ( appendonly='true', orientation='column') partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into resync_co_alter_part_split_partlist1 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from resync_co_alter_part_split_partlist1;


--
-- RESYNC CO TABLE 2
--
create table resync_co_alter_part_split_partlist2 (i int)  with ( appendonly='true', orientation='column') partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into resync_co_alter_part_split_partlist2 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from resync_co_alter_part_split_partlist2;


--
-- RESYNC CO TABLE 3
--
create table resync_co_alter_part_split_partlist3 (i int)  with ( appendonly='true', orientation='column') partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
--
-- Insert few records into the table
--
insert into resync_co_alter_part_split_partlist3 values (generate_series(1,10));
--
-- select from the Table
--
select count(*) from resync_co_alter_part_split_partlist3;



--
--ALTER SYNC1 CO TABLE
--
--
-- split partition
--
alter table sync1_co_alter_part_split_partlist6 split partition for(1) at (1,2) into (partition f1a, partition f1b);
--
-- Insert few records into the table
--
insert into sync1_co_alter_part_split_partlist6 values (generate_series(1,10));
--
-- Alter the table set distributed by 
--
Alter table sync1_co_alter_part_split_partlist6  set with ( reorganize='true') distributed randomly;
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_split_partlist6;

--
--ALTER CK_SYNC1 CO TABLE
--
--
-- split partition
--
alter table ck_sync1_co_alter_part_split_partlist5 split partition for(1) at (1,2) into (partition f1a, partition f1b);
--
-- Insert few records into the table
--
insert into ck_sync1_co_alter_part_split_partlist5 values (generate_series(1,10));
--
-- Alter the table set distributed by 
--
Alter table ck_sync1_co_alter_part_split_partlist5  set with ( reorganize='true') distributed randomly;
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_split_partlist5;

--
--ALTER CT CO TABLE
--
--
-- split partition
--
alter table ct_co_alter_part_split_partlist3 split partition for(1) at (1,2) into (partition f1a, partition f1b);
--
-- Insert few records into the table
--
insert into ct_co_alter_part_split_partlist3 values (generate_series(1,10));
--
-- Alter the table set distributed by 
--
Alter table ct_co_alter_part_split_partlist3  set with ( reorganize='true') distributed randomly;
--
-- select from the Table
--
select count(*) from ct_co_alter_part_split_partlist3;

--
--ALTER RESYNC CO TABLE
--
--
-- split partition
--
alter table resync_co_alter_part_split_partlist1 split partition for(1) at (1,2) into (partition f1a, partition f1b);
--
-- Insert few records into the table
--
insert into resync_co_alter_part_split_partlist1 values (generate_series(1,10));
--
-- Alter the table set distributed by 
--
Alter table resync_co_alter_part_split_partlist1  set with ( reorganize='true') distributed randomly;
--
-- select from the Table
--
select count(*) from resync_co_alter_part_split_partlist1;


