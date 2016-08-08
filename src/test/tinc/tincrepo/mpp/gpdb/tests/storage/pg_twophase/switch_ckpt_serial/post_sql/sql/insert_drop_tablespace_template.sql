-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLESPACE pg2_dropts_template FILESPACE filespace_test_a;
create table pg2_drop_table_ts_test_template(a int, b int) tablespace pg2_dropts_template;
insert into pg2_drop_table_ts_test_template select i,i+1 from generate_series(1,1000)i;
drop table pg2_drop_table_ts_test_template;
DROP TABLESPACE pg2_dropts_template;
