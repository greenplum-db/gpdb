-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CT CO TABLE 1
--

CREATE TABLE ct_co_alter_part_exchange_partrange1 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE ct_co_alter_part_exchange_partrange1_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into ct_co_alter_part_exchange_partrange1 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_partrange1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_partrange1;

--
-- CT CO TABLE 2
--

CREATE TABLE ct_co_alter_part_exchange_partrange2 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE ct_co_alter_part_exchange_partrange2_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into ct_co_alter_part_exchange_partrange2 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_partrange2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_partrange2;


--
-- CT CO TABLE 3
--

CREATE TABLE ct_co_alter_part_exchange_partrange3 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE ct_co_alter_part_exchange_partrange3_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into ct_co_alter_part_exchange_partrange3 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_partrange3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_partrange3;


--
-- CT CO TABLE 4
--

CREATE TABLE ct_co_alter_part_exchange_partrange4 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE ct_co_alter_part_exchange_partrange4_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into ct_co_alter_part_exchange_partrange4 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_partrange4_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_partrange4;


--
-- CT CO TABLE 5
--

CREATE TABLE ct_co_alter_part_exchange_partrange5 (

        unique1         int4,
        unique2         int4
)   with ( appendonly='true', orientation='column') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE ct_co_alter_part_exchange_partrange5_A (
        unique1         int4,
        unique2         int4) ;

--
-- Insert few records into the table
--
insert into ct_co_alter_part_exchange_partrange5 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_partrange5_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_partrange5;







--
-- ALTER SYNC1 CO 
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION RANGE
--
alter table sync1_co_alter_part_exchange_partrange4 exchange partition for (rank(1)) with table sync1_co_alter_part_exchange_partrange4_A;

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
-- ALTER CK_SYNC1 CO 
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION RANGE
--
alter table ck_sync1_co_alter_part_exchange_partrange3 exchange partition for (rank(1)) with table ck_sync1_co_alter_part_exchange_partrange3_A;

--
-- Insert few records into the table
--
insert into ck_sync1_co_alter_part_exchange_partrange3 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_co_alter_part_exchange_partrange3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_exchange_partrange3;
--
-- ALTER CT CO 
--
--
-- ALTER PARTITION TABLE EXCHANGE PARTITION RANGE
--
alter table ct_co_alter_part_exchange_partrange1 exchange partition for (rank(1)) with table ct_co_alter_part_exchange_partrange1_A;

--
-- Insert few records into the table
--
insert into ct_co_alter_part_exchange_partrange1 values ( generate_series(5,50),generate_series(15,60));
insert into ct_co_alter_part_exchange_partrange1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_co_alter_part_exchange_partrange1;

