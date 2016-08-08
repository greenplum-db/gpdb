-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
\d cr_ao_ctas
select count(*) from cr_ao_ctas;
drop table cr_ao_ctas;
drop table cr_seed_table_for_ao;
