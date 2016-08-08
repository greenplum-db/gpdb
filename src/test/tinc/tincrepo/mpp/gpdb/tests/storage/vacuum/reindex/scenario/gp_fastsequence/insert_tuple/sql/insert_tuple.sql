-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
insert into test_fastseqence select i , 'aa'||i from generate_series(1,100) i;
