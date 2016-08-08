-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP table IF EXISTS bar2;
set gp_select_invisible = true;
create table bar2 ( a int, b int) with (appendonly = true);
insert into bar2 select x , x from generate_series(1,10) as x;
INSERT 0 10
