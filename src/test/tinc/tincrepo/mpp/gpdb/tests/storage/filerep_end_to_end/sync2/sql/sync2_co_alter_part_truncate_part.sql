-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC2 CO TABLE 1
--

CREATE TABLE sync2_co_alter_part_truncate_part1 (
        unique1         int4,
        unique2         int4
)  with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync2_co_alter_part_truncate_part1_A (
        unique1         int4,
        unique2         int4)with ( appendonly='true', orientation='column') ;
--
-- Insert few records into the table
--
insert into sync2_co_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_co_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync2_co_alter_part_truncate_part1;


--
-- SYNC2 CO TABLE 2
--

CREATE TABLE sync2_co_alter_part_truncate_part2 (
        unique1         int4,
        unique2         int4
)  with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync2_co_alter_part_truncate_part2_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true', orientation='column');
--
-- Insert few records into the table
--
insert into sync2_co_alter_part_truncate_part2 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_co_alter_part_truncate_part2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync2_co_alter_part_truncate_part2;



--
-- ALTER SYNC1 CO
--

--
-- Truncate Partition
--
alter table sync1_co_alter_part_truncate_part7 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_truncate_part7 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_truncate_part7_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table sync1_co_alter_part_truncate_part7 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_truncate_part7;
--
-- Truncate default partition
--
alter table sync1_co_alter_part_truncate_part7 truncate default partition;
--
-- Insert few records into the table
--
insert into sync1_co_alter_part_truncate_part7 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_truncate_part7_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_truncate_part7;


--
-- ALTER CK_SYNC1 CO
--

--
-- Truncate Partition
--
alter table ck_sync1_co_alter_part_truncate_part6 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into ck_sync1_co_alter_part_truncate_part6 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_co_alter_part_truncate_part6_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table ck_sync1_co_alter_part_truncate_part6 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_truncate_part6;
--
-- Truncate default partition
--
alter table ck_sync1_co_alter_part_truncate_part6 truncate default partition;
--
-- Insert few records into the table
--
insert into ck_sync1_co_alter_part_truncate_part6 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_co_alter_part_truncate_part6_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_truncate_part6;

--
-- ALTER CT CO
--

--
-- Truncate Partition
--
alter table ct_co_alter_part_truncate_part4 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into ct_co_alter_part_truncate_part4 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_truncate_part4_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table ct_co_alter_part_truncate_part4 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from ct_co_alter_part_truncate_part4;
--
-- Truncate default partition
--
alter table ct_co_alter_part_truncate_part4 truncate default partition;
--
-- Insert few records into the table
--
insert into ct_co_alter_part_truncate_part4 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_truncate_part4_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_truncate_part4;

--
-- ALTER RESYNC CO
--

--
-- Truncate Partition
--
alter table resync_co_alter_part_truncate_part2 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into resync_co_alter_part_truncate_part2 values ( generate_series(5,50),generate_series(15,60));
insert into resync_co_alter_part_truncate_part2_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table resync_co_alter_part_truncate_part2 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from resync_co_alter_part_truncate_part2;
--
-- Truncate default partition
--
alter table resync_co_alter_part_truncate_part2 truncate default partition;
--
-- Insert few records into the table
--
insert into resync_co_alter_part_truncate_part2 values ( generate_series(5,50),generate_series(15,60));
insert into resync_co_alter_part_truncate_part2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_co_alter_part_truncate_part2;
--
-- ALTER SYNC2 CO
--

--
-- Truncate Partition
--
alter table sync2_co_alter_part_truncate_part1 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into sync2_co_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_co_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table sync2_co_alter_part_truncate_part1 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from sync2_co_alter_part_truncate_part1;
--
-- Truncate default partition
--
alter table sync2_co_alter_part_truncate_part1 truncate default partition;
--
-- Insert few records into the table
--
insert into sync2_co_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_co_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync2_co_alter_part_truncate_part1;

