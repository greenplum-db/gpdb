\c gptest;

select * from sto_heap1 order by col_numeric;

Create index sto_heap1_idx1 on sto_heap1(col_numeric);

select * from sto_heap1 order by col_numeric;

Create index sto_heap1_idx2 on sto_heap1 USING bitmap (col_text);

select * from sto_heap1 order by col_numeric;

Create unique index sto_heap1_idx3 on sto_heap1(col_unq);

select * from sto_heap1 order by col_numeric;

select count(*) from sto_heap2 ;

select count(*) from sto_heap3 ;

select count(*) from sto_heap4;

select * from sto_heap6 order by did;

Truncate sto_heap6;

select * from sto_heap6 order by did;

Drop index heap6_idx;

select * from sto_heap6 order by did;

Drop table sto_heap6;

