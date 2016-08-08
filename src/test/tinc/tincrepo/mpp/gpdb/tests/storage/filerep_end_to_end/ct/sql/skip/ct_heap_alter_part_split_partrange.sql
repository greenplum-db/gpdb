-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CT HEAP TABLE 1
--
create table ct_heap_alter_part_split_partrange1 (i int) partition by range(i) (start(1) end(10) every(5));
--
-- Insert few records into the table
--
insert into ct_heap_alter_part_split_partrange1 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_split_partrange1;

--
-- CT HEAP TABLE 2
--
create table ct_heap_alter_part_split_partrange2 (i int) partition by range(i) (start(1) end(10) every(5));
--
-- Insert few records into the table
--
insert into ct_heap_alter_part_split_partrange2 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_split_partrange2;


--
-- CT HEAP TABLE 3
--
create table ct_heap_alter_part_split_partrange3 (i int) partition by range(i) (start(1) end(10) every(5));
--
-- Insert few records into the table
--
insert into ct_heap_alter_part_split_partrange3 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_split_partrange3;



--
-- CT HEAP TABLE 4
--
create table ct_heap_alter_part_split_partrange4 (i int) partition by range(i) (start(1) end(10) every(5));
--
-- Insert few records into the table
--
insert into ct_heap_alter_part_split_partrange4 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_split_partrange4;



--
-- CT HEAP TABLE 5
--
create table ct_heap_alter_part_split_partrange5 (i int) partition by range(i) (start(1) end(10) every(5));
--
-- Insert few records into the table
--
insert into ct_heap_alter_part_split_partrange5 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_split_partrange5;



--
-- ALTER SYNC1 HEAP
--

--
-- Split Partition Range
--
alter table sync1_heap_alter_part_split_partrange4 split partition for(1) at (5) into (partition aa, partition bb);
--
-- Insert few records into the table
--
insert into sync1_heap_alter_part_split_partrange4 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from sync1_heap_alter_part_split_partrange4;


--
-- ALTER CK_SYNC1 HEAP
--

--
-- Split Partition Range
--
alter table ck_sync1_heap_alter_part_split_partrange3 split partition for(1) at (5) into (partition aa, partition bb);
--
-- Insert few records into the table
--
insert into ck_sync1_heap_alter_part_split_partrange3 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from ck_sync1_heap_alter_part_split_partrange3;


--
-- ALTER CT HEAP
--

--
-- Split Partition Range
--
alter table ct_heap_alter_part_split_partrange1 split partition for(1) at (5) into (partition aa, partition bb);
--
-- Insert few records into the table
--
insert into ct_heap_alter_part_split_partrange1 values (generate_series(1,9));
--
-- select from the Table
--
select count(*) from ct_heap_alter_part_split_partrange1;

