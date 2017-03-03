--start_ignore
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
drop table if exists m;
drop table if exists yyy;
--end_ignore

create table yyy(a int, b int) distributed randomly;
-- create master-only table
create table m(); 
alter table m add column a int;
alter table m add column b int;

-- generate data for m
insert into m select i, i%5 from generate_series(1,10)i;
