-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
\d pg2_heap_table_gist_index_b0

set enable_seqscan=off;
select property from pg2_heap_table_gist_index_b0 where property='( (0,0), (1,1) )';

DROP TABLE pg2_heap_table_gist_index_b0;
