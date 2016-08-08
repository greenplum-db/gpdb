CREATE TABLE abort_create_needed_cr_heap_ctas AS SELECT * FROM cr_seed_table;
select count(*) from abort_create_needed_cr_heap_ctas;
drop table abort_create_needed_cr_heap_ctas;
drop table cr_seed_table;
