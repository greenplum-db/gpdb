-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC AO TABLE 1
--
create table resync_ao_alter_part_split_partrange1 (i int)  with ( appendonly='true') partition by range(i) (start(1) end(10) every(5));
--
-- Insert few records into the table
--
insert into resync_ao_alter_part_split_partrange1 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from resync_ao_alter_part_split_partrange1;

--
-- RESYNC AO TABLE 2
--
create table resync_ao_alter_part_split_partrange2 (i int)  with ( appendonly='true') partition by range(i) (start(1) end(10) every(5));
--
-- Insert few records into the table
--
insert into resync_ao_alter_part_split_partrange2 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from resync_ao_alter_part_split_partrange2;


--
-- RESYNC AO TABLE 3
--
create table resync_ao_alter_part_split_partrange3 (i int)  with ( appendonly='true') partition by range(i) (start(1) end(10) every(5));
--
-- Insert few records into the table
--
insert into resync_ao_alter_part_split_partrange3 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from resync_ao_alter_part_split_partrange3;





--
-- ALTER SYNC1 AO
--

--
-- Split Partition Range
--
alter table sync1_ao_alter_part_split_partrange6 split partition for(1) at (5) into (partition aa, partition bb);
--
-- Insert few records into the table
--
insert into sync1_ao_alter_part_split_partrange6 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from sync1_ao_alter_part_split_partrange6;


--
-- ALTER CK_SYNC1 AO
--

--
-- Split Partition Range
--
alter table ck_sync1_ao_alter_part_split_partrange5 split partition for(1) at (5) into (partition aa, partition bb);
--
-- Insert few records into the table
--
insert into ck_sync1_ao_alter_part_split_partrange5 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_split_partrange5;


--
-- ALTER CT AO
--

--
-- Split Partition Range
--
alter table ct_ao_alter_part_split_partrange3 split partition for(1) at (5) into (partition aa, partition bb);
--
-- Insert few records into the table
--
insert into ct_ao_alter_part_split_partrange3 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_split_partrange3;


--
-- ALTER RESYNC AO
--

--
-- Split Partition Range
--
alter table resync_ao_alter_part_split_partrange1 split partition for(1) at (5) into (partition aa, partition bb);
--
-- Insert few records into the table
--
insert into resync_ao_alter_part_split_partrange1 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from resync_ao_alter_part_split_partrange1;

