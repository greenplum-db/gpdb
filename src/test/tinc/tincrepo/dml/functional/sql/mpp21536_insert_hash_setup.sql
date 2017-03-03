--start_ignore
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
drop table if exists m;
drop table if exists zzz;
--end_ignore

create table zzz(a int primary key, b int) distributed by (a);
-- create master-only table
create table m(); 
set allow_system_table_mods='DML';
delete from gp_distribution_policy where localoid='m'::regclass;
reset allow_system_table_mods;
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;
