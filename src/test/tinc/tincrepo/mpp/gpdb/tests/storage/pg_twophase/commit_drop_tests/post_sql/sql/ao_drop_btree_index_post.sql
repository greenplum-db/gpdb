CREATE INDEX cr_ao_btree_idx1 ON cr_ao_table_btree_index (numeric_col);
insert into cr_ao_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(101,200)i;
\d cr_ao_table_btree_index
set enable_seqscan=off;
select numeric_col from cr_ao_table_btree_index where numeric_col=1;
drop table cr_ao_table_btree_index;
