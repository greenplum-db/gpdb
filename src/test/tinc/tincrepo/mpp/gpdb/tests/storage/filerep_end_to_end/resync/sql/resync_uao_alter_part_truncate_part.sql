-- @product_version gpdb: [4.3.0.0-]
--
-- RESYNC UAO TABLE 1
--

CREATE TABLE resync_uao_alter_part_truncate_part1 (
        unique1         int4,
        unique2         int4
)  with ( appendonly='true') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE resync_uao_alter_part_truncate_part1_A (
        unique1         int4,
        unique2         int4)with ( appendonly='true') ;
--
-- Insert few records into the table
--
insert into resync_uao_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into resync_uao_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='resync_uao_alter_part_truncate_part1');
select count(*) AS only_visi_tups_ins  from resync_uao_alter_part_truncate_part1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from resync_uao_alter_part_truncate_part1;
set gp_select_invisible = false;
update resync_uao_alter_part_truncate_part1 set  unique2 = unique2 + 100   where unique1=6;
select count(*) AS only_visi_tups_upd  from resync_uao_alter_part_truncate_part1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from resync_uao_alter_part_truncate_part1;
set gp_select_invisible = false;
delete from resync_uao_alter_part_truncate_part1  where unique1>=7;
select count(*) AS only_visi_tups  from resync_uao_alter_part_truncate_part1;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from resync_uao_alter_part_truncate_part1;
set gp_select_invisible = false;

--
-- select from the Table
--
select count(*) from resync_uao_alter_part_truncate_part1;


--
-- RESYNC UAO TABLE 2
--

CREATE TABLE resync_uao_alter_part_truncate_part2 (
        unique1         int4,
        unique2         int4
)  with ( appendonly='true') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE resync_uao_alter_part_truncate_part2_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true');
--
-- Insert few records into the table
--
insert into resync_uao_alter_part_truncate_part2 values ( generate_series(5,50),generate_series(15,60));
insert into resync_uao_alter_part_truncate_part2_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_uao_alter_part_truncate_part2;


--
-- RESYNC UAO TABLE 3
--

CREATE TABLE resync_uao_alter_part_truncate_part3 (
        unique1         int4,
        unique2         int4
)  with ( appendonly='true') partition by range (unique1)
( partition aa start (0) end (500) every (100), default partition default_part );


CREATE TABLE resync_uao_alter_part_truncate_part3_A (
        unique1         int4,
        unique2         int4) with ( appendonly='true');
--
-- Insert few records into the table
--
insert into resync_uao_alter_part_truncate_part3 values ( generate_series(5,50),generate_series(15,60));
insert into resync_uao_alter_part_truncate_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_uao_alter_part_truncate_part3;



--
-- ALTER SYNC1 AO
--

--
-- Truncate Partition
--
alter table sync1_uao_alter_part_truncate_part6 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into sync1_uao_alter_part_truncate_part6 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_uao_alter_part_truncate_part6_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table sync1_uao_alter_part_truncate_part6 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from sync1_uao_alter_part_truncate_part6;
--
-- Truncate default partition
--
alter table sync1_uao_alter_part_truncate_part6 truncate default partition;
--
-- Insert few records into the table
--
insert into sync1_uao_alter_part_truncate_part6 values ( generate_series(5,50),generate_series(15,60));
insert into sync1_uao_alter_part_truncate_part6_A values ( generate_series(1,10),generate_series(21,30));
select count(*) FROM pg_appendonly WHERE visimaprelid is not NULL AND visimapidxid is not NULL AND relid in (SELECT oid FROM pg_class WHERE relname ='sync1_uao_alter_part_truncate_part6');
select count(*) AS only_visi_tups_ins  from sync1_uao_alter_part_truncate_part6;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups_ins  from sync1_uao_alter_part_truncate_part6;
set gp_select_invisible = false;
delete from sync1_uao_alter_part_truncate_part6  where unique1>=7;
select count(*) AS only_visi_tups  from sync1_uao_alter_part_truncate_part6;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from sync1_uao_alter_part_truncate_part6;
set gp_select_invisible = false;
--
-- select from the Table
--
select count(*) from sync1_uao_alter_part_truncate_part6;


--
-- ALTER CK_SYNC1 AO
--

--
-- Truncate Partition
--
alter table ck_sync1_uao_alter_part_truncate_part5 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into ck_sync1_uao_alter_part_truncate_part5 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_uao_alter_part_truncate_part5_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table ck_sync1_uao_alter_part_truncate_part5 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from ck_sync1_uao_alter_part_truncate_part5;
--
-- Truncate default partition
--
alter table ck_sync1_uao_alter_part_truncate_part5 truncate default partition;
--
-- Insert few records into the table
--
insert into ck_sync1_uao_alter_part_truncate_part5 values ( generate_series(5,50),generate_series(15,60));
insert into ck_sync1_uao_alter_part_truncate_part5_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ck_sync1_uao_alter_part_truncate_part5;

--
-- ALTER CT AO
--

--
-- Truncate Partition
--
alter table ct_uao_alter_part_truncate_part3 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into ct_uao_alter_part_truncate_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ct_uao_alter_part_truncate_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table ct_uao_alter_part_truncate_part3 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from ct_uao_alter_part_truncate_part3;
--
-- Truncate default partition
--
alter table ct_uao_alter_part_truncate_part3 truncate default partition;
--
-- Insert few records into the table
--
insert into ct_uao_alter_part_truncate_part3 values ( generate_series(5,50),generate_series(15,60));
insert into ct_uao_alter_part_truncate_part3_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from ct_uao_alter_part_truncate_part3;
--
-- ALTER RESYNC AO
--

--
-- Truncate Partition
--
alter table resync_uao_alter_part_truncate_part1 truncate partition for (rank(1));

--
-- Insert few records into the table
--
insert into resync_uao_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into resync_uao_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- Alter the table set distributed by 
--
Alter table resync_uao_alter_part_truncate_part1 set with ( reorganize='true') distributed by (unique2);
--
-- select from the Table
--
select count(*) from resync_uao_alter_part_truncate_part1;
--
-- Truncate default partition
--
alter table resync_uao_alter_part_truncate_part1 truncate default partition;
--
-- Insert few records into the table
--
insert into resync_uao_alter_part_truncate_part1 values ( generate_series(5,50),generate_series(15,60));
insert into resync_uao_alter_part_truncate_part1_A values ( generate_series(1,10),generate_series(21,30));
--
-- select from the Table
--
select count(*) from resync_uao_alter_part_truncate_part1;

