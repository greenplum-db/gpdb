INSERT INTO cr_ao_reindex_table_gist_index (id, property) VALUES (6, '( (0,0), (6,6) )');
\d cr_ao_reindex_table_gist_index
SELECT COUNT(*) FROM cr_ao_reindex_table_gist_index;

set enable_seqscan=off;
select property from cr_ao_reindex_table_gist_index where property='( (0,0), (6,6) )';
drop table cr_ao_reindex_table_gist_index;
