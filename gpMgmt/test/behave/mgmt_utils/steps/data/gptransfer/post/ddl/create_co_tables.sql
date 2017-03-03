\c gptest;

select * from sto_co1 order by col_numeric;

Create index sto_co1_idx1 on sto_co1(col_numeric);

select * from sto_co1 order by col_numeric;

Create index sto_co1_idx2 on sto_co1 USING bitmap (col_text);

select * from sto_co1 order by col_numeric;

select count(*) from sto_co2;

select count(*) from sto_co3;

select count(*) from sto_co4;

select * from sto_co6 order by did;

Truncate sto_co6;

select * from sto_co6 order by did;

Drop index co6_idx;

select * from sto_co6 order by did;

Drop table sto_co6;
