-- create range partitioned AO table
--start_ignore
 drop table if exists pt_ao_tab_rng cascade;
--end_ignore

 CREATE TABLE pt_ao_tab_rng (a int, b text, c int , d int, e numeric,success bool) with (appendonly=true,compresstype=quicklz, compresslevel=1)
 distributed by (a)
 partition by range(a)
 (
     start(1) end(20) every(5),
     default partition dft
 );

--Create indexes on the table
--start_ignore
drop index if exists ao_rng_idx1 cascade;
drop index if exists ao_rng_idx2 cascade;
--end_ignore

 -- partial index
 create index ao_rng_idx1 on pt_ao_tab_rng(a) where c > 10;

 -- expression index
 create index ao_rng_idx2 on pt_ao_tab_rng(upper(b));

--Drop partition
 Alter table pt_ao_tab_rng drop default partition; 

--ADD partitions
 alter table pt_ao_tab_rng add partition heap start(21) end(25) with (appendonly=false);
 alter table pt_ao_tab_rng add partition ao start(25) end(30) with (appendonly=true);
 alter table pt_ao_tab_rng add partition co start(31) end(35) with (appendonly=true,orientation=column);

 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'pt_ao_tab_rng_1_prt_heap', 'pt_ao_tab_rng_1_prt_ao','pt_ao_tab_rng_1_prt_co'));

--Split partition
 alter table pt_ao_tab_rng split partition heap at (23) into (partition heap1,partition heap2);
 alter table pt_ao_tab_rng split partition ao at (27) into (partition ao1,partition ao2);
 alter table pt_ao_tab_rng split partition co  at (33) into (partition co1,partition co2);
 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in ( 'pt_ao_tab_rng_1_prt_heap1' ,'pt_ao_tab_rng_1_prt_heap2' ,'pt_ao_tab_rng_1_prt_ao1', 'pt_ao_tab_rng_1_prt_ao2', 'pt_ao_tab_rng_1_prt_co1', 'pt_ao_tab_rng_1_prt_co2'));

--Exchange
 -- Create candidate table
--start_ignore
drop table if exists heap_can cascade;
drop table if exists ao_can cascade;
drop table if exists co_can cascade;
--end_ignore
  create table heap_can(like pt_ao_tab_rng);  
  create table ao_can(like pt_ao_tab_rng) with (appendonly=true);   
  create table co_can(like pt_ao_tab_rng)  with (appendonly=true,orientation=column);   

 alter table pt_ao_tab_rng add partition newco start(36) end(40) with (appendonly= true, orientation = column);
 alter table pt_ao_tab_rng add partition newheap start(40) end(45) with (appendonly= false);

 -- Exchange

 alter table pt_ao_tab_rng exchange partition newheap with table ao_can; -- HEAP <=> AO
 alter table pt_ao_tab_rng exchange partition ao1 with table co_can;-- AO <=> CO
 alter table pt_ao_tab_rng exchange partition newco with table heap_can; --CO <=> HEAP

 \d+ ao_can 
 \d+ co_can
 \d+ heap_can

-- Create more index indexes
--start_ignore
drop index if exists ao_rng_idx4 cascade;
drop index if exists ao_rng_idx3 cascade;
--end_ignore

 create index ao_rng_idx3 on pt_ao_tab_rng(c,d) where a = 40 OR a = 50; -- multicol indx
 CREATE INDEX ao_rng_idx4 ON pt_ao_tab_rng ((b || ' ' || e)); --Expression

--Add default partition
 alter table pt_ao_tab_rng add default partition dft;

--Split default partition
 alter table pt_ao_tab_rng split default partition start(45) end(60) into (partition dft, partition two);
