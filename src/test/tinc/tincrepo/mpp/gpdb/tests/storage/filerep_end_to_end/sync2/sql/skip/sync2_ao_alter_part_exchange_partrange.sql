-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC2 AO TABLE 1
--

CREATE TABLE sync2_ao_alter_part_exchange_partrange1 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync2_ao_alter_part_exchange_partrange1_A (
        unique1         int4,
        unique2         int4)  with ( appendonly='true');

--
-- Insert few records into the table
--
insert into sync2_ao_alter_part_exchange_partrange1 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_ao_alter_part_exchange_partrange1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync2_ao_alter_part_exchange_partrange1;

--
-- SYNC2 AO TABLE 2
--

CREATE TABLE sync2_ao_alter_part_exchange_partrange2 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync2_ao_alter_part_exchange_partrange2_A (
        unique1         int4,
        unique2         int4)  with ( appendonly='true');

--
-- Insert few records into the table
--
insert into sync2_ao_alter_part_exchange_partrange2 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_ao_alter_part_exchange_partrange2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync2_ao_alter_part_exchange_partrange2;




--
-- ALTER SYNC1 AO 
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION RANGE
--
alter table sync1_ao_alter_part_exchange_partrange7 exchange partition for (rank(1)) with table sync1_ao_alter_part_exchange_partrange7_A;

--
-- Insert few records into the table
--
insert into sync1_ao_alter_part_exchange_partrange7 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_ao_alter_part_exchange_partrange7_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_ao_alter_part_exchange_partrange7;

--
-- ALTER CK_SYNC1 AO 
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION RANGE
--
alter table ck_sync1_ao_alter_part_exchange_partrange6 exchange partition for (rank(1)) with table ck_sync1_ao_alter_part_exchange_partrange6_A;

--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_exchange_partrange6 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_ao_alter_part_exchange_partrange6_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_exchange_partrange6;

--
-- ALTER CT AO 
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION RANGE
--
alter table ct_ao_alter_part_exchange_partrange4 exchange partition for (rank(1)) with table ct_ao_alter_part_exchange_partrange4_A;

--
-- Insert few records into the table
--
insert into ct_ao_alter_part_exchange_partrange4 values ( generate_series(5,50),generate_series(15,60));
insert into ct_ao_alter_part_exchange_partrange4_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_exchange_partrange4;

--
-- ALTER RESYNC AO 
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION RANGE
--
alter table resync_ao_alter_part_exchange_partrange2 exchange partition for (rank(1)) with table resync_ao_alter_part_exchange_partrange2_A;

--
-- Insert few records into the table
--
insert into resync_ao_alter_part_exchange_partrange2 values ( generate_series(5,50),generate_series(15,60));
insert into resync_ao_alter_part_exchange_partrange2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_ao_alter_part_exchange_partrange2;
--
-- ALTER SYNC2 AO 
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION RANGE
--
alter table sync2_ao_alter_part_exchange_partrange1 exchange partition for (rank(1)) with table sync2_ao_alter_part_exchange_partrange1_A;

--
-- Insert few records into the table
--
insert into sync2_ao_alter_part_exchange_partrange1 values ( generate_series(5,50),generate_series(15,60));
insert into sync2_ao_alter_part_exchange_partrange1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync2_ao_alter_part_exchange_partrange1;

