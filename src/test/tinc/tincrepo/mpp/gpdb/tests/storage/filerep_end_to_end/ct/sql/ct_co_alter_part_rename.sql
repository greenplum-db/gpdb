-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CT CO TABLE 1
--
CREATE TABLE ct_co_alter_part_rn1 (id int, rank int, year date, gender
char(1))   with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ct_co_alter_part_rn1 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_co_alter_part_rn1;

--
-- CT CO TABLE 2
--
CREATE TABLE ct_co_alter_part_rn2 (id int, rank int, year date, gender
char(1))   with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ct_co_alter_part_rn2 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_co_alter_part_rn2;


--
-- CT CO TABLE 3
--
CREATE TABLE ct_co_alter_part_rn3 (id int, rank int, year date, gender
char(1))   with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ct_co_alter_part_rn3 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_co_alter_part_rn3;


--
-- CT CO TABLE 4
--
CREATE TABLE ct_co_alter_part_rn4 (id int, rank int, year date, gender
char(1))   with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ct_co_alter_part_rn4 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_co_alter_part_rn4;


--
-- CT CO TABLE 5
--
CREATE TABLE ct_co_alter_part_rn5 (id int, rank int, year date, gender
char(1))   with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ct_co_alter_part_rn5 values (generate_series(1,20),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_co_alter_part_rn5;



--
-- ALTER SYNC1 CO
--

--
-- Add default Partition
--
alter table sync1_co_alter_part_rn4 add default partition default_part;
--
-- Rename Default Partition
--
alter table sync1_co_alter_part_rn4 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_rn4 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_co_alter_part_rn4;

--
-- Rename Table
--
alter table sync1_co_alter_part_rn4 rename to  sync1_co_alter_part_rn4_0;

--
-- Insert few records into the table
--
insert into sync1_co_alter_part_rn4_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from sync1_co_alter_part_rn4_0;
--
-- ALTER CK_SYNC1 CO
--

--
-- Add default Partition
--
alter table ck_sync1_co_alter_part_rn3 add default partition default_part;
--
-- Rename Default Partition
--
alter table ck_sync1_co_alter_part_rn3 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into ck_sync1_co_alter_part_rn3 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_rn3;

--
-- Rename Table
--
alter table ck_sync1_co_alter_part_rn3 rename to  ck_sync1_co_alter_part_rn3_0;

--
-- Insert few records into the table
--
insert into ck_sync1_co_alter_part_rn3_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ck_sync1_co_alter_part_rn3_0;

--
-- ALTER CT CO
--

--
-- Add default Partition
--
alter table ct_co_alter_part_rn1 add default partition default_part;
--
-- Rename Default Partition
--
alter table ct_co_alter_part_rn1 rename default partition to new_default_part;

--
-- Insert few records into the table
--
insert into ct_co_alter_part_rn1 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_co_alter_part_rn1;

--
-- Rename Table
--
alter table ct_co_alter_part_rn1 rename to  ct_co_alter_part_rn1_0;

--
-- Insert few records into the table
--
insert into ct_co_alter_part_rn1_0 values (generate_series(1,10),1,'2001-01-01','F');

--
-- select from the Table
--
select count(*) from ct_co_alter_part_rn1_0;
