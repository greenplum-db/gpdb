-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE INDEX cr_ao_gist_idx1 ON cr_ao_table_gist_index USING GiST (property);
\d cr_ao_table_gist_index

set enable_seqscan=off;
select property from cr_ao_table_gist_index where property='( (0,0), (1,1) )';

DROP TABLE cr_ao_table_gist_index;
