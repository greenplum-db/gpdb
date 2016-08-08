-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

-- CO table

select count(*) from sto_altcp1;

select count(*) from sto_altcp2;

select count(*) from sto_altcp3;

select * from sto_altcp1 order by b;

select * from sto_altce1 order by a1;

select * from sto_altae1 order by a1;

-- AO table

select count(*) from sto_altap1;

select count(*) from sto_altap2;

select count(*) from sto_altap3;

select * from sto_altap1 order by b;

-- Heap table

select count(*) from sto_althp1;

select count(*) from sto_althp2;

select count(*) from sto_althp3;

select * from sto_althp1 order by b;

select * from sto_althe1 order by a1;

--Mixed
select * from sto_altmsp1 order by col1,col2,col3;
