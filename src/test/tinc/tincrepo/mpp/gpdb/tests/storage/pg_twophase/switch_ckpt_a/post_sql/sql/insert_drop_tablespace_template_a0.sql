-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLESPACE pg2_dropts_a0 FILESPACE filespace_test_a;
create table pg2_drop_table_ts_test_a0(a int, b int) tablespace pg2_dropts_a0;
insert into pg2_drop_table_ts_test_a0 select i,i+1 from generate_series(1,1000)i;
drop table pg2_drop_table_ts_test_a0;
DROP TABLESPACE pg2_dropts_a0;
