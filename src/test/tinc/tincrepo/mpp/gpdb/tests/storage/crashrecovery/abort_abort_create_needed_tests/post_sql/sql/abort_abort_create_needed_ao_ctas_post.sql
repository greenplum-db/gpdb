CREATE TABLE abort_create_needed_cr_ao_ctas with (appendonly=true) AS SELECT * FROM cr_seed_table_for_ao;
select count(*) from abort_create_needed_cr_ao_ctas;
drop table abort_create_needed_cr_ao_ctas;
drop table cr_seed_table_for_ao;
