-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
drop table if exists test_table;
create table test_table (id int, col1 int, col2 int, name text) with (appendonly=true, orientation=column);

drop index if exists index_mpp24168;
create index index_mpp24168 on test_table using btree(id);

