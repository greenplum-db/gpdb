-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
create table r1 (a int);
create table ao (a int) with (appendonly=true);
insert into ao values (generate_series(1,100));