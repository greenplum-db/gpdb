-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC AO Part Table 1
--
CREATE TABLE resync_ao_alter_part_add_default_part1 (id int, name text, rank int, year date, gender
char(1))  with ( appendonly='true') DISTRIBUTED BY (id, gender, year)
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
insert into resync_ao_alter_part_add_default_part1 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- RESYNC AO Part Table 2
--
CREATE TABLE resync_ao_alter_part_add_default_part2 (id int, name text, rank int, year date, gender
char(1))  with ( appendonly='true') DISTRIBUTED BY (id, gender, year)
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
insert into resync_ao_alter_part_add_default_part2 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- RESYNC AO Part Table 3
--
CREATE TABLE resync_ao_alter_part_add_default_part3 (id int, name text, rank int, year date, gender
char(1))  with ( appendonly='true') DISTRIBUTED BY (id, gender, year)
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
insert into resync_ao_alter_part_add_default_part3 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
--
-- ALTER TABLE TO ADD DEFAULT PART
--
--
--
-- ALTER SYNC1 AO Part Add Default Parition
--
alter table sync1_ao_alter_part_add_default_part6 add default partition default_part;
--
-- Insert few records into the table
--
insert into sync1_ao_alter_part_add_default_part6 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- ALTER CK_SYNC1 AO Part Add Default Parition
--
alter table ck_sync1_ao_alter_part_add_default_part5 add default partition default_part;
--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_add_default_part5 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- ALTER CT AO Part Add Default Parition
--
alter table ct_ao_alter_part_add_default_part3 add default partition default_part;
--
-- Insert few records into the table
--
insert into ct_ao_alter_part_add_default_part3 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- ALTER RESYNC AO Part Add Default Parition
--
alter table resync_ao_alter_part_add_default_part1 add default partition default_part;
--
-- Insert few records into the table
--
insert into resync_ao_alter_part_add_default_part1 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
