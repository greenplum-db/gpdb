-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE cr_heap_ctas AS SELECT * FROM cr_seed_table;
select count(*) from cr_heap_ctas;
drop table cr_heap_ctas;
drop table cr_seed_table;
