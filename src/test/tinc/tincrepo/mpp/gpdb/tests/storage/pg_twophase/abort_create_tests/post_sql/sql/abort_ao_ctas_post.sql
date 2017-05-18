-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE cr_ao_ctas WITH (appendonly=true) AS SELECT * FROM cr_seed_table_for_ao;
select count(*) from cr_ao_ctas;
drop table cr_ao_ctas;
drop table cr_seed_table_for_ao;
