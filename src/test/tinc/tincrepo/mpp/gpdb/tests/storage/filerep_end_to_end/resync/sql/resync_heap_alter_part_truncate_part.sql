-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC HEAP TABLE 1
--

CREATE TABLE resync_heap_alter_part_truncate_part1 (
        unique1         int4,
        unique2         int4
) partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE resync_heap_alter_part_truncate_part1_A (
        unique1         int4,
        unique2         int4) ;
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into resync_heap_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_heap_alter_part_truncate_part1;


--
-- RESYNC HEAP TABLE 2
--

CREATE TABLE resync_heap_alter_part_truncate_part2 (
        unique1         int4,
        unique2         int4
) partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE resync_heap_alter_part_truncate_part2_A (
        unique1         int4,
        unique2         int4) ;
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_truncate_part2 values ( generate_series(5,50),generate_series(15,60));
insert into resync_heap_alter_part_truncate_part2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_heap_alter_part_truncate_part2;


--
-- RESYNC HEAP TABLE 3
--

CREATE TABLE resync_heap_alter_part_truncate_part3 (
        unique1         int4,
        unique2         int4
) partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE resync_heap_alter_part_truncate_part3_A (
        unique1         int4,
        unique2         int4) ;
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_truncate_part3 values ( generate_series(5,50),generate_series(15,60));
insert into resync_heap_alter_part_truncate_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_heap_alter_part_truncate_part3;



--
-- ALTER SYNC1 HEAP
--

--
-- Truncate Partition
--
alter table sync1_heap_alter_part_truncate_part6 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_truncate_part6 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_heap_alter_part_truncate_part6_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table sync1_heap_alter_part_truncate_part6 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_truncate_part6;
--
-- Truncate default partition
--
alter table sync1_heap_alter_part_truncate_part6 truncate default partition;
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_truncate_part6 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_heap_alter_part_truncate_part6_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_truncate_part6;


--
-- ALTER CK_SYNC1 HEAP
--

--
-- Truncate Partition
--
alter table ck_sync1_heap_alter_part_truncate_part5 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into ck_sync1_heap_alter_part_truncate_part5 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_heap_alter_part_truncate_part5_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table ck_sync1_heap_alter_part_truncate_part5 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_truncate_part5;
--
-- Truncate default partition
--
alter table ck_sync1_heap_alter_part_truncate_part5 truncate default partition;
--
-- Insert few records into the table
--
insert into ck_sync1_heap_alter_part_truncate_part5 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_heap_alter_part_truncate_part5_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_truncate_part5;

--
-- ALTER CT HEAP
--

--
-- Truncate Partition
--
alter table ct_heap_alter_part_truncate_part3 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into ct_heap_alter_part_truncate_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ct_heap_alter_part_truncate_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table ct_heap_alter_part_truncate_part3 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_truncate_part3;
--
-- Truncate default partition
--
alter table ct_heap_alter_part_truncate_part3 truncate default partition;
--
-- Insert few records into the table
--
insert into ct_heap_alter_part_truncate_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ct_heap_alter_part_truncate_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_truncate_part3;
--
-- ALTER RESYNC HEAP
--

--
-- Truncate Partition
--
alter table resync_heap_alter_part_truncate_part1 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into resync_heap_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into resync_heap_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table resync_heap_alter_part_truncate_part1 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from resync_heap_alter_part_truncate_part1;
--
-- Truncate default partition
--
alter table resync_heap_alter_part_truncate_part1 truncate default partition;
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into resync_heap_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_heap_alter_part_truncate_part1;

