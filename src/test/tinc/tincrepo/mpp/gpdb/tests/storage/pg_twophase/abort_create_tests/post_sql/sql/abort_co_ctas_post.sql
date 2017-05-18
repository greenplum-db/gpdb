-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE cr_co_ctas WITH (appendonly=true,orientation=column) AS SELECT * FROM cr_seed_table_for_co;
select count(*) from cr_co_ctas;
drop table cr_co_ctas;
drop table cr_seed_table_for_co;
