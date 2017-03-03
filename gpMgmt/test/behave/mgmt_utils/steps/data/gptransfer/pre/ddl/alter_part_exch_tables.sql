\c gptest;

-- @Description Alter echange partitions Heap/AO and CO
-- @Author Divya Sivanandan

-- Heap partition table to be exchaged
--start_ignore
Drop table if exists sto_althe1 ;
--end_ignore
Create table sto_althe1 ( a1 int, a2 int, a3 text) partition by range(a1) 
        (partition p1 start(1) end(5),
        partition p2 start(5) end(10),
        partition p3 start(10) end(15),
        default partition others);
    
insert into sto_althe1 values(generate_series(1,20), 100, 'heap table to be exchanged');

-- AO partition table to be exchnged
--start_ignore
Drop table if exists sto_altae1 ;
--end_ignore
Create table sto_altae1 ( a1 int, a2 int, a3 text) with(appendonly=true)  partition by range(a1) 
        (partition p1 start(1) end(5),
        partition p2 start(5) end(10),
        partition p3 start(10) end(15),
        default partition others);
    
insert into sto_altae1 values(generate_series(1,20), 100, 'ao table to be exchanged');


-- CO partition table to be exchanged
--start_ignore
Drop table if exists sto_altce1 ;
--end_ignore
Create table sto_altce1 ( a1 int, a2 int, a3 text) with(appendonly=true, orientation=column) partition by range(a1) 
        (partition p1 start(1) end(5),
        partition p2 start(5) end(10),
        partition p3 start(10) end(15),
        default partition others);
    
insert into sto_altce1 values(generate_series(1,20), 100, 'co table to be exchanged');


-- Exchange with Heap table
--start_ignore
Drop table if exists sto_eh_ao;
--end_ignore
Create table sto_eh_ao(a1 int, a2 int, a3 text) with(appendonly=true);
insert into sto_eh_ao values(6,100,'ao table');

--start_ignore
Drop table if exists sto_eh_co;
--end_ignore
Create table sto_eh_co(a1 int, a2 int, a3 text) with(appendonly=true, orientation=column);
insert into sto_eh_co values(12,100,'co table');

--start_ignore
Drop table if exists sto_eh_def;
--end_ignore
Create table sto_eh_def(a1 int, a2 int, a3 text) with(appendonly=true);
insert into sto_eh_def values(17,100,'ao table');

-- Exchange with AO table
--start_ignore
Drop table if exists sto_ea_heap;
--end_ignore
Create table sto_ea_heap(a1 int, a2 int, a3 text);
insert into sto_ea_heap values(3,100,'heap table');

--start_ignore
Drop table if exists sto_ea_co;
--end_ignore
Create table sto_ea_co(a1 int, a2 int, a3 text) with(appendonly=true, orientation=column);
insert into sto_ea_co values(12,100,'co table');

--start_ignore
Drop table if exists sto_ea_def;
--end_ignore
Create table sto_ea_def(a1 int, a2 int, a3 text) with(appendonly=true, orientation=column);
insert into sto_ea_def values(17,100,'co table');

-- Exchange with CO table
--start_ignore
Drop table if exists sto_ec_heap;
--end_ignore
Create table sto_ec_heap(a1 int, a2 int, a3 text);
insert into sto_ec_heap values(3,100,'heap table');

--start_ignore
Drop table if exists sto_ec_ao;
--end_ignore
Create table sto_ec_ao(a1 int, a2 int, a3 text) with(appendonly=true);
insert into sto_ec_ao values(12,100,'co table');

--start_ignore
Drop table if exists sto_ec_def;
--end_ignore
Create table sto_ec_def(a1 int, a2 int, a3 text);
insert into sto_ec_def values(17,100,'co table');
