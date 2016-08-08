-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC2 Heap Part Table 1
--
CREATE TABLE sync2_heap_alter_part_add_default_part1 (id int, name text, rank int, year date, gender
char(1)) DISTRIBUTED BY (id, gender, year)
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
insert into sync2_heap_alter_part_add_default_part1 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- SYNC2 Heap Part Table 2
--
CREATE TABLE sync2_heap_alter_part_add_default_part2 (id int, name text, rank int, year date, gender
char(1)) DISTRIBUTED BY (id, gender, year)
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
insert into sync2_heap_alter_part_add_default_part2 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
--
-- ALTER TABLE TO ADD DEFAULT PART
--
--
--
-- ALTER SYNC1 Heap Part Add Default Parition
--
alter table sync1_heap_alter_part_add_default_part7 add default partition default_part;
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_add_default_part7 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- ALTER CK_SYNC1 Heap Part Add Default Parition
--
alter table ck_sync1_heap_alter_part_add_default_part6 add default partition default_part;
--
-- Insert few records into the table
--
insert into ck_sync1_heap_alter_part_add_default_part6 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- ALTER CT Heap Part Add Default Parition
--
alter table ct_heap_alter_part_add_default_part4 add default partition default_part;
--
-- Insert few records into the table
--
insert into ct_heap_alter_part_add_default_part4 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- ALTER RESYNC Heap Part Add Default Parition
--
alter table resync_heap_alter_part_add_default_part2 add default partition default_part;
--
-- Insert few records into the table
--
insert into resync_heap_alter_part_add_default_part2 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
--
-- ALTER SYNC2 Heap Part Add Default Parition
--
alter table sync2_heap_alter_part_add_default_part1 add default partition default_part;
--
-- Insert few records into the table
--
insert into sync2_heap_alter_part_add_default_part1 values (generate_series(1,100),'ann',generate_series(1,100),'2001-01-01','F');
