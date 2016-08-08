-- @Description Alter echange partitions Heap/AO and CO


-- Heap partition table to be exchaged
Drop table if exists sto_althe1 ;
Create table sto_althe1 ( a1 int, a2 int, a3 text) partition by range(a1) 
        (partition p1 start(1) end(5),
        partition p2 start(5) end(10),
        partition p3 start(10) end(15),
        default partition others);
    
insert into sto_althe1 values(generate_series(1,20), 100, 'heap table to be exchanged');

-- AO partition table to be exchnged
Drop table if exists sto_altae1 ;
Create table sto_altae1 ( a1 int, a2 int, a3 text) with(appendonly=true)  partition by range(a1) 
        (partition p1 start(1) end(5),
        partition p2 start(5) end(10),
        partition p3 start(10) end(15),
        default partition others);
    
insert into sto_altae1 values(generate_series(1,20), 100, 'ao table to be exchanged');


-- CO partition table to be exchanged
Drop table if exists sto_altce1 ;
Create table sto_altce1 ( a1 int, a2 int, a3 text) with(appendonly=true, orientation=column) partition by range(a1) 
        (partition p1 start(1) end(5),
        partition p2 start(5) end(10),
        partition p3 start(10) end(15),
        default partition others);
    
insert into sto_altce1 values(generate_series(1,20), 100, 'co table to be exchanged');


-- Exchange with Heap table
Drop table if exists sto_eh_ao;
Create table sto_eh_ao(a1 int, a2 int, a3 text) with(appendonly=true);
insert into sto_eh_ao values(6,100,'ao table');

Drop table if exists sto_eh_co;
Create table sto_eh_co(a1 int, a2 int, a3 text) with(appendonly=true, orientation=column);
insert into sto_eh_co values(12,100,'co table');

Drop table if exists sto_eh_def;
Create table sto_eh_def(a1 int, a2 int, a3 text) with(appendonly=true);
insert into sto_eh_def values(17,100,'ao table');


--Alter exchange heep default part with ao table
Alter table sto_althe1 exchange default partition with table sto_eh_def;

-- Alter exchange heap part with ao table
Alter table sto_althe1 exchange partition p2 with table sto_eh_ao;

-- Alter exchange heap part with co table
Alter table sto_althe1 exchange partition p3 with table sto_eh_co;

select * from sto_althe1 order by a1;
-- Exchange with AO table
Drop table if exists sto_ea_heap;
Create table sto_ea_heap(a1 int, a2 int, a3 text);
insert into sto_ea_heap values(3,100,'heap table');

Drop table if exists sto_ea_co;
Create table sto_ea_co(a1 int, a2 int, a3 text) with(appendonly=true, orientation=column);
insert into sto_ea_co values(12,100,'co table');

Drop table if exists sto_ea_def;
Create table sto_ea_def(a1 int, a2 int, a3 text) with(appendonly=true, orientation=column);
insert into sto_ea_def values(17,100,'co table');

--Alter exchange ao default part with co table
Alter table sto_altae1 exchange default partition with table sto_ea_def;

-- Alter exchange ao part with heap table
Alter table sto_altae1 exchange partition p1 with table sto_ea_heap;

-- Alter exchange ao part with co table
Alter table sto_altae1 exchange partition p3 with table sto_ea_co;

select * from sto_altae1 order by a1;

-- Exchange with CO table
Drop table if exists sto_ec_heap;
Create table sto_ec_heap(a1 int, a2 int, a3 text);
insert into sto_ec_heap values(3,100,'heap table');

Drop table if exists sto_ec_ao;
Create table sto_ec_ao(a1 int, a2 int, a3 text) with(appendonly=true);
insert into sto_ec_ao values(12,100,'co table');

Drop table if exists sto_ec_def;
Create table sto_ec_def(a1 int, a2 int, a3 text);
insert into sto_ec_def values(17,100,'co table');

--Alter exchange ao default part with heap table
Alter table sto_altce1 exchange default partition with table sto_ec_def;

-- Alter exchange co part with heap table
Alter table sto_altce1 exchange partition p1 with table sto_ec_heap;

-- Alter exchange co part with ao table
Alter table sto_altce1 exchange partition p3 with table sto_ec_ao;

select * from sto_altce1 order by a1;
