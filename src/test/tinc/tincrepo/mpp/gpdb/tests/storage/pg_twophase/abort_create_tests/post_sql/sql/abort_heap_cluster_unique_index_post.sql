-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CLUSTER cr_cluster_unq_idx1 ON cr_cluster_table_unique_index;
\d cr_cluster_table_unique_index
 insert into cr_cluster_table_unique_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(101,200)i;

select count(*) from cr_cluster_table_unique_index;

set enable_seqscan=off;
select numeric_col from cr_cluster_table_unique_index where numeric_col=1;
drop table cr_cluster_table_unique_index;
