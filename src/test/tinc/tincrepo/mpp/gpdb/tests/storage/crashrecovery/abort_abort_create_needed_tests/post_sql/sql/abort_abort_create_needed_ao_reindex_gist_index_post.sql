REINDEX INDEX abort_create_needed_cr_ao_reindex_gist_idx1;
INSERT INTO abort_create_needed_cr_ao_reindex_table_gist_index (id, property) VALUES (6, '( (0,0), (6,6) )');
SELECT COUNT(*) FROM abort_create_needed_cr_ao_reindex_table_gist_index;
drop table abort_create_needed_cr_ao_reindex_table_gist_index;
