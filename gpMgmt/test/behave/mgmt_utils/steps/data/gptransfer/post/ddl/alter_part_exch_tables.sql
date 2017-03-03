\c gptest;

--Alter exchange heep default part with ao table
Alter table sto_althe1 exchange default partition with table sto_eh_def;

-- Alter exchange heap part with ao table
Alter table sto_althe1 exchange partition p2 with table sto_eh_ao;

-- Alter exchange heap part with co table
Alter table sto_althe1 exchange partition p3 with table sto_eh_co;

select * from sto_althe1 order by a1;

--Alter exchange ao default part with co table
Alter table sto_altae1 exchange default partition with table sto_ea_def;

-- Alter exchange ao part with heap table
Alter table sto_altae1 exchange partition p1 with table sto_ea_heap;

-- Alter exchange ao part with co table
Alter table sto_altae1 exchange partition p3 with table sto_ea_co;

select * from sto_altae1 order by a1;

--Alter exchange ao default part with heap table
Alter table sto_altce1 exchange default partition with table sto_ec_def;

-- Alter exchange co part with heap table
Alter table sto_altce1 exchange partition p1 with table sto_ec_heap;

-- Alter exchange co part with ao table
Alter table sto_altce1 exchange partition p3 with table sto_ec_ao;

select * from sto_altce1 order by a1;
