\d cr_cluster_table_gist_index
INSERT INTO cr_cluster_table_gist_index (id, property) VALUES (6, '( (0,0), (6,6) )');
select count(*) from cr_cluster_table_gist_index;

set enable_seqscan=off;
select property from cr_cluster_table_gist_index where property='( (0,0), (6,6) )';
drop table cr_cluster_table_gist_index;
