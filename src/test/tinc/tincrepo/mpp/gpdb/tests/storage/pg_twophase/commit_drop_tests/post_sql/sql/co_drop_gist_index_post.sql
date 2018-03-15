CREATE INDEX cr_co_gist_idx1 ON cr_co_table_gist_index USING GiST (property);
set enable_seqscan=off;
select property from cr_co_table_gist_index where property='( (0,0), (1,1) )';
\d cr_co_table_gist_index
DROP TABLE cr_co_table_gist_index;
