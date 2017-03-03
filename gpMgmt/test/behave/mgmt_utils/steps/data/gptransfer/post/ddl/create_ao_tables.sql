\c gptest;

select * from sto_ao1 order by col_numeric;

Create index sto_ao1_idx1 on sto_ao1(col_numeric);

select * from sto_ao1 order by col_numeric;

Create index sto_ao1_idx2 on sto_ao1 USING bitmap (col_text);

select * from sto_ao1 order by col_numeric;

select count(*) from sto_ao4;

select * from sto_ao6 order by did;

Truncate sto_ao6;

select * from sto_ao6 order by did;

Drop index ao6_idx;

select * from sto_ao6 order by did;

Drop table sto_ao6;
