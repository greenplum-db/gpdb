CREATE TABLE abort_create_needed_cr_co_ctas with (appendonly=true) AS SELECT * FROM cr_seed_table_for_co;
select count(*) from abort_create_needed_cr_co_ctas;
drop table abort_create_needed_cr_co_ctas;
drop table cr_seed_table_for_co;
