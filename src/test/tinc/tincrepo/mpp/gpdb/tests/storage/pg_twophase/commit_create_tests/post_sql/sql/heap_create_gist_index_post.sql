-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
set enable_seqscan=off;
select property from cr_heap_table_gist_index where property='( (0,0), (1,1) )';

\d cr_heap_table_gist_index

DROP TABLE cr_heap_table_gist_index;
