-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

select count(*) from sto_heap2;

select count(*) from sto_ao2;

select count(*) from sto_co2;

select * from sto_heap1 order by col_numeric;

select * from sto_ao1 order by col_numeric;

select * from sto_co1 order by col_numeric;

-- Part tables

select count(*) from sto_heap_p1 ;

select count(*) from sto_ao_p1 ;

select count(*) from sto_co_p1;

select count(*) from sto_mx_p1;


select * from sto_aocomp_p1 order by b;

select count(*) from sto_cocomp_p1;

select * from sto_cocomp_p2 order by col_int1;

