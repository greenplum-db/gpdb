-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- start_ignore
drop table if exists insert_to_select_tbl;
drop table if exists insert_to_select_tbl_new;
-- end_ignore
create table insert_to_select_tbl(i int);
insert into insert_to_select_tbl select * from generate_series(1,100);
create table insert_to_select_tbl_new (i int);
insert into insert_to_select_tbl_new select * from insert_to_select_tbl; 
