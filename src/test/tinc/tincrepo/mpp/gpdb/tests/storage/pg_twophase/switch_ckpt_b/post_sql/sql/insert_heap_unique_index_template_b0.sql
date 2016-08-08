-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
\d pg2_heap_table_unique_index_b0

insert into pg2_heap_table_unique_index_b0 values ('0_b0', 0, '0_b0', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into pg2_heap_table_unique_index_b0 values ('1_b0', 1, '1_b0', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into pg2_heap_table_unique_index_b0 values ('2_b0', 2, '2_b0', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into pg2_heap_table_unique_index_b0 select i||'_'||repeat('template',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

set enable_seqscan=off;
select numeric_col from pg2_heap_table_unique_index_b0 where numeric_col=1;
drop table pg2_heap_table_unique_index_b0;
