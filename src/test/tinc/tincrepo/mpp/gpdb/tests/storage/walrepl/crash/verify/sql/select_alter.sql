-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- CO table
select * from sto_alt_co1 order by bigint_col;

select * from sto_alt_co4 order by bigint_col;

select count(*) from sto_alt_co2;

select count(*) from sto_alt_co3;

-- AO table
select * from sto_alt_ao1 order by bigint_col;

select * from sto_alt_ao4 order by bigint_col;

select count(*) from sto_alt_ao2;

select count(*) from sto_alt_ao3;

-- Heap table
select * from sto_alt_heap1 order by bigint_col;

select * from sto_alt_heap4 order by bigint_col;

select count(*) from sto_alt_heap2;

select count(*) from sto_alt_heap3;
