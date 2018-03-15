set enable_seqscan=off;
select property from cr_heap_table_gist_index where property='( (0,0), (1,1) )';

\d cr_heap_table_gist_index

DROP TABLE cr_heap_table_gist_index;
