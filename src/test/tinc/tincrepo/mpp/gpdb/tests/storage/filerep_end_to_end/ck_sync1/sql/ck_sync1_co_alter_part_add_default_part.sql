-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CK_SYNC1 CO Part Table 1
--
CREATE TABLE ck_sync1_co_alter_part_add_default_part1 (id int, name text, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_default_part1 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- CK_SYNC1 CO Part Table 2
--
CREATE TABLE ck_sync1_co_alter_part_add_default_part2 (id int, name text, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_default_part2 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- CK_SYNC1 CO Part Table 3
--
CREATE TABLE ck_sync1_co_alter_part_add_default_part3 (id int, name text, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_default_part3 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- CK_SYNC1 CO Part Table 4
--
CREATE TABLE ck_sync1_co_alter_part_add_default_part4 (id int, name text, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_default_part4 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- CK_SYNC1 CO Part Table 5
--
CREATE TABLE ck_sync1_co_alter_part_add_default_part5 (id int, name text, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_default_part5 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- CK_SYNC1 CO Part Table 6
--
CREATE TABLE ck_sync1_co_alter_part_add_default_part6 (id int, name text, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_default_part6 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- CK_SYNC1 CO Part Table 7
--
CREATE TABLE ck_sync1_co_alter_part_add_default_part7 (id int, name text, rank int, year date, gender
char(1))  with ( appendonly='true', orientation='column') DISTRIBUTED BY (id, gender, year)
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
insert into ck_sync1_co_alter_part_add_default_part7 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');

--
--
-- ALTER TABLE TO ADD DEFAULT PART
--
--
--
-- ALTER SYNC1 CO Part Add Default Parition
--
alter table sync1_co_alter_part_add_default_part2 add default partition default_part;
--
-- Insert few records into the table
--
insert into sync1_co_alter_part_add_default_part2 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');

--
-- ALTER CK_SYNC1 CO Part Add Default Parition
--
alter table ck_sync1_co_alter_part_add_default_part1 add default partition default_part;
--
-- Insert few records into the table
--
insert into ck_sync1_co_alter_part_add_default_part1 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');




