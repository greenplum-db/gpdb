\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
drop table if exists r;
create table r (a int primary key, b int) distributed by (a);
insert into r values(1,1);
