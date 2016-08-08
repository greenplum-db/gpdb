-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
\d cr_heap_ctas
select count(*) from cr_heap_ctas;
drop table cr_heap_ctas;
drop table cr_seed_table;
