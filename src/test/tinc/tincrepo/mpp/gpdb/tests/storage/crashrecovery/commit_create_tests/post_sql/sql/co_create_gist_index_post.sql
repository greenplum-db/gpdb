\d cr_co_table_gist_index

set enable_seqscan=off;
select property from cr_co_table_gist_index where property='( (0,0), (1,1) )';

DROP TABLE cr_co_table_gist_index;
