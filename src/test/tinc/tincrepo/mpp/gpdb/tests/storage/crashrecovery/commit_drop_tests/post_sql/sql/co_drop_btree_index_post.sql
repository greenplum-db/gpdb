\d cr_co_table_btree_index
CREATE INDEX cr_co_btree_idx1 ON cr_co_table_btree_index (numeric_col);
\d cr_co_table_btree_index
insert into cr_co_table_btree_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(101,200)i;
set enable_seqscan=off;
select numeric_col from cr_co_table_btree_index where numeric_col=1;
drop table cr_co_table_btree_index;
