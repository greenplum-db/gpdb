begin;
CREATE TABLE abort_create_needed_cr_heap_ctas AS SELECT * FROM cr_seed_table;
drop table abort_create_needed_cr_heap_ctas;
commit;
