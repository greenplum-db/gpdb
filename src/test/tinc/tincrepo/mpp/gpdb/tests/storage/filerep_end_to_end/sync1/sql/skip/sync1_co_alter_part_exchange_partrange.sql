-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC1 CO TABLE 1
--

CREATE TABLE sync1_co_alter_part_exchange_partrange1 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync1_co_alter_part_exchange_partrange1_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_partrange1 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_partrange1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_partrange1;

--
-- SYNC1 CO TABLE 2
--

CREATE TABLE sync1_co_alter_part_exchange_partrange2 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync1_co_alter_part_exchange_partrange2_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_partrange2 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_partrange2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_partrange2;


--
-- SYNC1 CO TABLE 3
--

CREATE TABLE sync1_co_alter_part_exchange_partrange3 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync1_co_alter_part_exchange_partrange3_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_partrange3 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_partrange3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_partrange3;


--
-- SYNC1 CO TABLE 4
--

CREATE TABLE sync1_co_alter_part_exchange_partrange4 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync1_co_alter_part_exchange_partrange4_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_partrange4 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_partrange4_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_partrange4;


--
-- SYNC1 CO TABLE 5
--

CREATE TABLE sync1_co_alter_part_exchange_partrange5 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync1_co_alter_part_exchange_partrange5_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_partrange5 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_partrange5_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_partrange5;


--
-- SYNC1 CO TABLE 6
--

CREATE TABLE sync1_co_alter_part_exchange_partrange6 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync1_co_alter_part_exchange_partrange6_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_partrange6 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_partrange6_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_partrange6;


--
-- SYNC1 CO TABLE 7
--

CREATE TABLE sync1_co_alter_part_exchange_partrange7 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync1_co_alter_part_exchange_partrange7_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_partrange7 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_partrange7_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_partrange7;


--
-- SYNC1 CO TABLE 1
--

CREATE TABLE sync1_co_alter_part_exchange_partrange8 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE sync1_co_alter_part_exchange_partrange8_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_partrange8 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_partrange8_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_partrange8;





--
-- ALTER SYNC1 CO 
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION RANGE
--
alter table sync1_co_alter_part_exchange_partrange1 exchange partition for (rank(1)) with table sync1_co_alter_part_exchange_partrange1_A;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_exchange_partrange1 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_co_alter_part_exchange_partrange1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from sync1_co_alter_part_exchange_partrange1;

