-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC2 CO TABLE 1
--
CREATE TABLE sync2_co_alter_part_exchange_default_part1 (
        unique1         int4,
        unique2         int4
)     with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );



CREATE TABLE sync2_co_alter_part_exchange_default_part1_A (
        unique1         int4,
        unique2         int4)  with ( appendonly='true', orientation='column') ;
--
-- Insert few records into the table
--
insert into sync2_co_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_co_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));



--
-- SYNC2 CO TABLE 2
--
CREATE TABLE sync2_co_alter_part_exchange_default_part2 (
        unique1         int4,
        unique2         int4
)     with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );



CREATE TABLE sync2_co_alter_part_exchange_default_part2_A (
        unique1         int4,
        unique2         int4)  with ( appendonly='true', orientation='column') ;
--
-- Insert few records into the table
--
insert into sync2_co_alter_part_exchange_default_part2 values ( generate_series(5,50),generate_series(15,60));
insert into  sync2_co_alter_part_exchange_default_part2_A values ( generate_series(1,10),generate_series(21,30));

--
--
-- ALTER SYNC1 CO EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table sync1_co_alter_part_exchange_default_part7 exchange default partition with table sync1_co_alter_part_exchange_default_part7_A;
--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_default_part7 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_default_part7_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_default_part7;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table sync1_co_alter_part_exchange_default_part7 exchange default partition with table sync1_co_alter_part_exchange_default_part7_A with validation;
--
-- Insert few records into the table
--

insert into sync1_co_alter_part_exchange_default_part7 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_default_part7_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_default_part7;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table sync1_co_alter_part_exchange_default_part7 exchange default partition with table sync1_co_alter_part_exchange_default_part7_A without validation;

--
-- Insert few records into the table
--

insert into sync1_co_alter_part_exchange_default_part7 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_default_part7_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_default_part7;


--
--
-- ALTER CK_SYNC1 CO EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ck_sync1_co_alter_part_exchange_default_part6 exchange default partition with table ck_sync1_co_alter_part_exchange_default_part6_A;
--
-- Insert few records into the table
--
insert into ck_sync1_co_alter_part_exchange_default_part6 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_co_alter_part_exchange_default_part6_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_exchange_default_part6;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table ck_sync1_co_alter_part_exchange_default_part6 exchange default partition with table ck_sync1_co_alter_part_exchange_default_part6_A with validation;
--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_exchange_default_part6 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_co_alter_part_exchange_default_part6_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_exchange_default_part6;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ck_sync1_co_alter_part_exchange_default_part6 exchange default partition with table ck_sync1_co_alter_part_exchange_default_part6_A without validation;

--
-- Insert few records into the table
--

insert into ck_sync1_co_alter_part_exchange_default_part6 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_co_alter_part_exchange_default_part6_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_exchange_default_part6;

--
--
-- ALTER CT CO EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ct_co_alter_part_exchange_default_part4 exchange default partition with table ct_co_alter_part_exchange_default_part4_A;
--
-- Insert few records into the table
--
insert into ct_co_alter_part_exchange_default_part4 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_default_part4_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_default_part4;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table ct_co_alter_part_exchange_default_part4 exchange default partition with table ct_co_alter_part_exchange_default_part4_A with validation;
--
-- Insert few records into the table
--

insert into ct_co_alter_part_exchange_default_part4 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_default_part4_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_default_part4;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table ct_co_alter_part_exchange_default_part4 exchange default partition with table ct_co_alter_part_exchange_default_part4_A without validation;

--
-- Insert few records into the table
--

insert into ct_co_alter_part_exchange_default_part4 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_default_part4_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_default_part4;


--
--
-- ALTER RESYNC CO EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table resync_co_alter_part_exchange_default_part2 exchange default partition with table resync_co_alter_part_exchange_default_part2_A;
--
-- Insert few records into the table
--
insert into resync_co_alter_part_exchange_default_part2 values ( generate_series(5,50),generate_series(15,60));
insert into resync_co_alter_part_exchange_default_part2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_co_alter_part_exchange_default_part2;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table resync_co_alter_part_exchange_default_part2 exchange default partition with table resync_co_alter_part_exchange_default_part2_A with validation;
--
-- Insert few records into the table
--

insert into resync_co_alter_part_exchange_default_part2 values ( generate_series(5,50),generate_series(15,60));
insert into resync_co_alter_part_exchange_default_part2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_co_alter_part_exchange_default_part2;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table resync_co_alter_part_exchange_default_part2 exchange default partition with table resync_co_alter_part_exchange_default_part2_A without validation;

--
-- Insert few records into the table
--

insert into resync_co_alter_part_exchange_default_part2 values ( generate_series(5,50),generate_series(15,60));
insert into resync_co_alter_part_exchange_default_part2_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from resync_co_alter_part_exchange_default_part2;
--
--
-- ALTER SYNC2 CO EXCHANGE DEFAULT PART
--
--

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table sync2_co_alter_part_exchange_default_part1 exchange default partition with table sync2_co_alter_part_exchange_default_part1_A;
--
-- Insert few records into the table
--
insert into sync2_co_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_co_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync2_co_alter_part_exchange_default_part1;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION 
--
alter table sync2_co_alter_part_exchange_default_part1 exchange default partition with table sync2_co_alter_part_exchange_default_part1_A with validation;
--
-- Insert few records into the table
--

insert into sync2_co_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_co_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync2_co_alter_part_exchange_default_part1;

--
-- ALTER PARTITION TABLE EXCHANGE DEFAULT PARTITION
--
alter table sync2_co_alter_part_exchange_default_part1 exchange default partition with table sync2_co_alter_part_exchange_default_part1_A without validation;

--
-- Insert few records into the table
--

insert into sync2_co_alter_part_exchange_default_part1 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_co_alter_part_exchange_default_part1_A values ( generate_series(1,10),generate_series(21,30));

--
-- select from the Table
--
select count(*) from sync2_co_alter_part_exchange_default_part1;


