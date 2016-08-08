begin;
CREATE TABLE abort_create_needed_cr_co_ctas with (appendonly = true,orientation=column) AS SELECT * FROM cr_seed_table_for_co;
drop table abort_create_needed_cr_co_ctas;
commit;
