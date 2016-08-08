-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS ao2;
create table ao2 (i int, j varchar(10)) with (appendonly=true, orientation=column) distributed by (i);
