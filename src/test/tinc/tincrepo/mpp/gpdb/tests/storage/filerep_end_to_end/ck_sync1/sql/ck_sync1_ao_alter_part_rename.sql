-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CK_SYNC1 AO TABLE 1
--
CREATE TABLE ck_sync1_ao_alter_part_rn1 (id int, rank int, year date, gender
char(1))   with ( appendonly='true') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_rn1 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_rn1;

--
-- CK_SYNC1 AO TABLE 2
--
CREATE TABLE ck_sync1_ao_alter_part_rn2 (id int, rank int, year date, gender
char(1))   with ( appendonly='true') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_rn2 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_rn2;


--
-- CK_SYNC1 AO TABLE 3
--
CREATE TABLE ck_sync1_ao_alter_part_rn3 (id int, rank int, year date, gender
char(1))   with ( appendonly='true') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_rn3 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_rn3;


--
-- CK_SYNC1 AO TABLE 4
--
CREATE TABLE ck_sync1_ao_alter_part_rn4 (id int, rank int, year date, gender
char(1))   with ( appendonly='true') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_rn4 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_rn4;


--
-- CK_SYNC1 AO TABLE 5
--
CREATE TABLE ck_sync1_ao_alter_part_rn5 (id int, rank int, year date, gender
char(1))   with ( appendonly='true') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_rn5 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_rn5;


--
-- CK_SYNC1 AO TABLE 6
--
CREATE TABLE ck_sync1_ao_alter_part_rn6 (id int, rank int, year date, gender
char(1))   with ( appendonly='true') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_rn6 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_rn6;


--
-- CK_SYNC1 AO TABLE 7
--
CREATE TABLE ck_sync1_ao_alter_part_rn7 (id int, rank int, year date, gender
char(1))   with ( appendonly='true') DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_rn7 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_rn7;

--
-- ALTER SYNC1 AO
--

--
-- Add default Partition
--
alter table sync1_ao_alter_part_rn2 add default partition default_part;
--
-- Rename Default Partition
--
alter table sync1_ao_alter_part_rn2 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into sync1_ao_alter_part_rn2 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_ao_alter_part_rn2;

--
-- Rename Table
--
alter table sync1_ao_alter_part_rn2 rename to  sync1_ao_alter_part_rn2_0;

--
-- Insert few records into the table
--
insert into sync1_ao_alter_part_rn2_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_ao_alter_part_rn2_0;
--
-- ALTER CK_SYNC1 AO
--

--
-- Add default Partition
--
alter table ck_sync1_ao_alter_part_rn1 add default partition default_part;
--
-- Rename Default Partition
--
alter table ck_sync1_ao_alter_part_rn1 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_rn1 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_rn1;

--
-- Rename Table
--
alter table ck_sync1_ao_alter_part_rn1 rename to  ck_sync1_ao_alter_part_rn1_0;

--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_rn1_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_rn1_0;
