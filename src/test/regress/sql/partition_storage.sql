--Create a heap table with partitions ( having diff storage parameters)
--start_ignore
drop table if exists pt_heap_tab cascade;
--end_ignore
 Create table  pt_heap_tab(a int, b text, c int , d int, e numeric,success bool) with ( appendonly=false )
 distributed by (a)
 partition by list(b)
 (
          partition abc values ('abc','abc1','abc2') with (appendonly=false), -- HEAP
          partition def values ('def','def1','def3') with (appendonly=true, compresslevel=1), 
          partition ghi values ('ghi','ghi1','ghi2') with (appendonly=true), -- AO
          default partition dft
 );

--Create indexes on the table
 -- Partial index
--start_ignore
drop index if exists heap_idx1 cascade;
drop index if exists heap_idx2 cascade;
--end_ignore
 create index heap_idx1 on pt_heap_tab(a) where c > 10;

 -- Expression index
 create index heap_idx2 on pt_heap_tab(upper(b));

--Drop partition
 alter table pt_heap_tab drop default partition;

--Add partition
 alter table pt_heap_tab add partition xyz values ('xyz','xyz1','xyz2') WITH (appendonly=true, orientation=column, compresslevel=5); -- CO

 alter table pt_heap_tab add partition jkl values ('jkl','jkl1','jkl2') WITH (appendonly=true); -- AO

 alter table pt_heap_tab add partition mno values ('mno','mno1','mno2') WITH (appendonly=false); --Heap

--Check properties of the added partition tables
 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'pt_heap_tab_1_prt_xyz', 'pt_heap_tab_1_prt_jkl','pt_heap_tab_1_prt_mno'));

--Insert Data
 insert into pt_heap_tab select 1, 'xyz', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_heap_tab select 1, 'abc', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_heap_tab select 1, 'def', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_heap_tab select 1, 'ghi', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_heap_tab select 1, 'jkl', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_heap_tab select 1, 'mno', 1, 1, 1.0 , true from generate_series(1, 10);

--Split partition [Creates new partitions to be of the same type as the parent partition. All heap partitions created]
 alter table pt_heap_tab split partition abc at ('abc1') into ( partition abc1,partition abc2); -- Heap
 alter table pt_heap_tab split partition ghi at ('ghi1') into ( partition ghi1,partition ghi2); --AO
 alter table pt_heap_tab split partition xyz at ('xyz1') into ( partition xyz1,partition xyz2); --CO

--Check the storage type and properties of the split partition
 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in ('pt_heap_tab_1_prt_xyz1','pt_heap_tab_1_prt_xyz2','pt_heap_tab_1_prt_ghi1','pt_heap_tab_1_prt_ghi2','pt_heap_tab_1_prt_abc1','pt_heap_tab_1_prt_abc2'));

--Exchange partition
 -- Create candidate table
--start_ignore
drop table if exists heap_can cascade;
drop table if exists ao_can cascade;
drop table if exists co_can cascade;
--end_ignore
 create table heap_can(like pt_heap_tab);  
 create table ao_can(like pt_heap_tab) with (appendonly=true);   
 create table co_can(like pt_heap_tab)  with (appendonly=true,orientation=column);   

 -- Exchange
 alter table pt_heap_tab exchange partition for ('abc') with table ao_can ; -- Heap exchanged with  AO
 alter table pt_heap_tab add partition pqr values ('pqr','pqr1','pqr2') WITH (appendonly=true, orientation=column, compresslevel=5);
 alter table pt_heap_tab exchange partition for ('def') with table co_can; -- AO exchanged with CO
 alter table pt_heap_tab exchange partition for ('pqr') with table heap_can; -- CO exchanged with Heap

--Check for the storage properties and indexes of the two tables involved in the exchange
 \d+ heap_can
 \d+ co_can
 \d+ ao_can

--Further operations
 alter table pt_heap_tab drop partition jkl;
 truncate table pt_heap_tab;

--Further create some more indexes
--start_ignore
drop index if exists heap_idx3 cascade;
drop index if exists heap_idx4 cascade;
--end_ignore
 create index heap_idx3 on pt_heap_tab(c,d) where a = 40 OR a = 50; -- multicol indx
 CREATE INDEX heap_idx4 ON pt_heap_tab ((b || ' ' || e)); --Expression

--Add default partition
 alter table pt_heap_tab add default partition dft;

--Split default partition
 alter table pt_heap_tab split default partition at ('uvw') into (partition dft, partition uvw);
--Create an AO table with partitions ( having diff storage parameters)
--start_ignore
 drop table if exists pt_ao_tab cascade;
--end_ignore
 Create table  pt_ao_tab(a int, b text, c int , d int, e numeric,success bool) with ( appendonly=true )
 distributed by (a)
 partition by list(b)
 (
          partition abc values ('abc','abc1','abc2') with (appendonly=false), -- HEAP
          partition def values ('def','def1','def3') with (appendonly=true, compresslevel=1), 
          partition ghi values ('ghi','ghi1','ghi2') with (appendonly=true), -- AO
          default partition dft
 );

--Create indexes on the table
 -- Partial index
--start_ignore
drop index if exists ao_idx1 cascade;
drop index if exists ao_idx2 cascade;
--end_ignore
 create index ao_idx1 on pt_ao_tab(a) where c > 10;

 -- Expression index
 create index ao_idx2 on pt_ao_tab(upper(b));

--Drop partition
 alter table pt_ao_tab drop default partition;

--Add partition
 alter table pt_ao_tab add partition xyz values ('xyz','xyz1','xyz2') WITH (appendonly=true,orientation=column,compresslevel=5); --CO

 alter table pt_ao_tab add partition jkl values ('jkl','jkl1','jkl2') WITH (appendonly=true); -- AO

 alter table pt_ao_tab add partition mno values ('mno','mno1','mno2') WITH (appendonly=false); --Heap

--Check properties of the added partition tables
 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'pt_ao_tab_1_prt_xyz', 'pt_ao_tab_1_prt_jkl','pt_ao_tab_1_prt_mno'));

--Insert Data
 insert into pt_ao_tab select 1, 'xyz', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_ao_tab select 1, 'abc', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_ao_tab select 1, 'def', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_ao_tab select 1, 'ghi', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_ao_tab select 1, 'jkl', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_ao_tab select 1, 'mno', 1, 1, 1.0 , true from generate_series(1, 10);

--Split partition [Creates new partitions to be of the same type as the parent partition. All heap partitions created]
 alter table pt_ao_tab split partition abc at ('abc1') into ( partition abc1,partition abc2); -- Heap
 alter table pt_ao_tab split partition ghi at ('ghi1') into ( partition ghi1,partition ghi2); --AO
 alter table pt_ao_tab split partition xyz at ('xyz1') into ( partition xyz1,partition xyz2); --CO

--Check the storage type and properties of the split partition
 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in (select oid from pg_class where relname in ('pt_ao_tab_1_prt_xyz1','pt_ao_tab_1_prt_xyz2','pt_ao_tab_1_prt_ghi1','pt_ao_tab_1_prt_ghi2','pt_ao_tab_1_prt_abc1','pt_ao_tab_1_prt_abc2'));

--Exchange partition
 -- Create candidate table
--start_ignore
drop table if exists heap_can cascade;
drop table if exists ao_can cascade;
drop table if exists co_can cascade;
--end_ignore

 create table heap_can(like pt_ao_tab);  
 create table ao_can(like pt_ao_tab) with (appendonly=true);   
 create table co_can(like pt_ao_tab)  with (appendonly=true,orientation=column);   

 -- Exchange
 alter table pt_ao_tab add partition pqr values ('pqr','pqr1','pqr2') WITH (appendonly=true,orientation=column,compresslevel=5);-- CO
 alter table pt_ao_tab add partition stu values ('stu','stu1','stu2') WITH (appendonly=false);-- heap
 alter table pt_ao_tab exchange partition for ('stu') with table ao_can ;-- Heap tab exchanged with  AO
 alter table pt_ao_tab exchange partition for ('def') with table co_can; --AO tab exchanged with CO
 alter table pt_ao_tab exchange partition for ('pqr') with table heap_can; --CO tab exchanged with Heap

--Check for the storage properties and indexes of the two tables involved in the exchange
 \d+ heap_can
 \d+ co_can
 \d+ ao_can

--Further operations
 alter table pt_ao_tab drop partition jkl;
 truncate table pt_ao_tab;

--Further create some more indexes
--start_ignore
drop index if exists ao_idx4 cascade;
drop index if exists ao_idx3 cascade;
--end_ignore
 create index ao_idx3 on pt_ao_tab(c,d) where a = 40 OR a = 50; -- multicol indx
 CREATE INDEX ao_idx4 ON pt_ao_tab ((b || ' ' || e)); --Expression

--Add default partition
 alter table pt_ao_tab add default partition dft;

--Split default partition
 alter table pt_ao_tab split default partition at ('uvw') into (partition dft, partition uvw);
--Create an CO table with partitions ( having diff storage parameters)
--start_ignore
 drop table if exists pt_co_tab cascade;
--end_ignore
Create table  pt_co_tab(a int, b text, c int , d int, e numeric,success bool) with ( appendonly = true, orientation = column)
 distributed by (a)
 partition by list(b)
 (
          partition abc values ('abc','abc1','abc2') with (appendonly=false), -- HEAP
          partition def values ('def','def1','def3') with (appendonly=true, compresslevel=1), 
          partition ghi values ('ghi','ghi1','ghi2') with (appendonly=true), -- AO
          default partition dft
 );

--Create indexes on the table
--start_ignore
drop index if exists co_idx1 cascade;
drop index if exists co_idx2 cascade;
--end_ignore
 -- Partial index
 create index co_idx1 on pt_co_tab(a) where c > 10;

 -- Expression index
 create index co_idx2 on pt_co_tab(upper(b));

--Drop partition
 alter table pt_co_tab drop default partition;

--Add partition
 alter table pt_co_tab add partition xyz values ('xyz','xyz1','xyz2') WITH (appendonly=true,orientation=column,compresslevel=5); --CO

 alter table pt_co_tab add partition jkl values ('jkl','jkl1','jkl2') WITH (appendonly=true,compresslevel=1); -- AO

 alter table pt_co_tab add partition mno values ('mno','mno1','mno2') WITH (appendonly=false); --Heap

--Check properties of the added partition tables
 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'pt_co_tab_1_prt_xyz', 'pt_co_tab_1_prt_jkl','pt_co_tab_1_prt_mno'));

--Insert Data
 insert into pt_co_tab select 1, 'xyz', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_co_tab select 1, 'abc', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_co_tab select 1, 'def', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_co_tab select 1, 'ghi', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_co_tab select 1, 'jkl', 1, 1, 1.0 , true from generate_series(1, 10);
 insert into pt_co_tab select 1, 'mno', 1, 1, 1.0 , true from generate_series(1, 10);

--Split partition [Creates new partitions to be of the same type as the parent partition. All heap partitions created]
 alter table pt_co_tab split partition abc at ('abc1') into ( partition abc1,partition abc2); -- Heap
 alter table pt_co_tab split partition ghi at ('ghi1') into ( partition ghi1,partition ghi2); --AO
 alter table pt_co_tab split partition xyz at ('xyz1') into ( partition xyz1,partition xyz2); --CO

--Check the storage type and properties of the split partition
 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in (select oid from pg_class where relname in ('pt_co_tab_1_prt_xyz1','pt_co_tab_1_prt_xyz2','pt_co_tab_1_prt_ghi1','pt_co_tab_1_prt_ghi2','pt_co_tab_1_prt_abc1','pt_co_tab_1_prt_abc2'));

--Exchange partition
 -- Create candidate table
--start_ignore
drop table if exists heap_can cascade;
drop table if exists ao_can cascade;
drop table if exists co_can cascade;
--end_ignore

 create table heap_can(like pt_co_tab);  
 create table ao_can(like pt_co_tab) with (appendonly=true);   
 create table co_can(like pt_co_tab)  with (appendonly=true,orientation=column);   

 -- Exchange
 alter table pt_co_tab add partition pqr values ('pqr','pqr1','pqr2') WITH (appendonly=true,compresslevel=5);-- AO
 alter table pt_co_tab add partition stu values ('stu','stu1','stu2') WITH (appendonly=false);-- heap
 alter table pt_co_tab exchange partition for ('stu') with table ao_can ; -- Heap exchanged with AO
 alter table pt_co_tab exchange partition for ('pqr') with table co_can; -- AO exchanged with CO
 alter table pt_co_tab exchange partition for ('xyz1') with table heap_can; -- CO exchanged with Heap

--Check for the storage properties and indexes of the two tables involved in the exchange
 \d+ heap_can
 \d+ co_can
 \d+ ao_can

-- Further operations
 alter table pt_co_tab drop partition jkl;
 truncate table pt_co_tab;

--Further create some more indexes
 create index idx3 on pt_co_tab(c,d) where a = 40 OR a = 50; -- multicol indx
 CREATE INDEX idx5 ON pt_co_tab ((b || ' ' || e)); --Expression

--Add default partition
 alter table pt_co_tab add default partition dft;

--Split default partition
 alter table pt_co_tab split default partition at ('uvw') into (partition dft, partition uvw);
-- create range partitioned Heap table
--start_ignore
 drop table if exists pt_heap_tab_rng cascade;
--end_ignore
 CREATE TABLE pt_heap_tab_rng (a int, b text, c int , d int, e numeric,success bool) with (appendonly=false)
 distributed by (a)
 partition by range(a)
 (
     start(1) end(20) every(5),
     default partition dft
 );

-- Create indexes on the table
--start_ignore
drop index if exists heap_rng_idx1 cascade;
drop index if exists heap_rng_idx2 cascade;
--end_ignore

 -- partial index
 create index heap_rng_idx1 on pt_heap_tab_rng(a) where c > 10;

 -- expression index
 create index heap_rng_idx2 on pt_heap_tab_rng(upper(b));

-- Drop partition
 Alter table pt_heap_tab_rng drop default partition; 

-- ADD partitions
 alter table pt_heap_tab_rng add partition heap start(21) end(25) with (appendonly=false);
 alter table pt_heap_tab_rng add partition ao start(25) end(30) with (appendonly=true);
 alter table pt_heap_tab_rng add partition co start(31) end(35) with (appendonly=true,orientation=column);

 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'pt_heap_tab_rng_1_prt_heap', 'pt_heap_tab_rng_1_prt_ao','pt_heap_tab_rng_1_prt_co'));

-- Split partition
 alter table pt_heap_tab_rng split partition heap at (23) into (partition heap1,partition heap2);
 alter table pt_heap_tab_rng split partition ao at (27) into (partition ao1,partition ao2);
 alter table pt_heap_tab_rng split partition co  at (33) into (partition co1,partition co2);
 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in ( 'pt_heap_tab_rng_1_prt_heap1' ,'pt_heap_tab_rng_1_prt_heap2' ,'pt_heap_tab_rng_1_prt_ao1', 'pt_heap_tab_rng_1_prt_ao2', 'pt_heap_tab_rng_1_prt_co1', 'pt_heap_tab_rng_1_prt_co2'));

-- Exchange
 -- Create candidate table
--start_ignore
  drop table if exists heap_can;
  drop table if exists ao_can;
  drop table if exists co_can;
--end_ignore

  create table heap_can(like pt_heap_tab_rng);  
  create table ao_can(like pt_heap_tab_rng) with (appendonly=true);   
  create table co_can(like pt_heap_tab_rng)  with (appendonly=true,orientation=column);   

 alter table pt_heap_tab_rng add partition newco start(36) end(40) with (appendonly= true, orientation = column);
 alter table pt_heap_tab_rng add partition newao start(40) end(45) with (appendonly= true);

 -- Exchange

 alter table pt_heap_tab_rng exchange partition heap1 with table ao_can; -- HEAP <=> AO
 alter table pt_heap_tab_rng exchange partition newao with table co_can; -- AO <=> CO
 alter table pt_heap_tab_rng exchange partition newco with table heap_can; -- CO <=> HEAP

 \d+ ao_can 
 \d+ co_can
 \d+ heap_can
-- Create more index indexes
--start_ignore
drop index if exists heap_rng_idx4 cascade;
drop index if exists heap_rng_idx3 cascade;
--end_ignore
 create index heap_rng_idx3 on pt_heap_tab_rng(c,d) where a = 40 OR a = 50; -- multicol indx
 CREATE INDEX heap_rng_idx4 ON pt_heap_tab_rng ((b || ' ' || e)); --Expression

-- Add default partition
 alter table pt_heap_tab_rng add default partition dft;

--Split default partition
 alter table pt_heap_tab_rng split default partition start(45) end(60) into (partition dft, partition two);
-- create range partitioned AO table
--start_ignore
 drop table if exists pt_ao_tab_rng cascade;
--end_ignore

 CREATE TABLE pt_ao_tab_rng (a int, b text, c int , d int, e numeric,success bool) with (appendonly=true,compresstype=zlib, compresslevel=1)
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
--create range partitioned CO table
--start_ignore
 drop table if exists pt_co_tab_rng cascade;
--end_ignore
 CREATE TABLE pt_co_tab_rng (a int, b text, c int , d int, e numeric,success bool) with (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1)
 distributed by (a)
 partition by range(a)
 (
     start(1) end(20) every(5),
     default partition dft
 );

--Create indexes on the table
--start_ignore
drop index if exists co_rng_idx1 cascade;
drop index if exists co_rng_idx2 cascade;
--end_ignore

 -- partial index
 create index co_rng_idx1 on pt_co_tab_rng(a) where c > 10;

 -- expression index
 create index co_rng_idx2 on pt_co_tab_rng(upper(b));

--Drop partition
 Alter table pt_co_tab_rng drop default partition; 

--ADD partitions
 alter table pt_co_tab_rng add partition heap start(21) end(25) with (appendonly=false);
 alter table pt_co_tab_rng add partition ao start(25) end(30) with (appendonly=true);
 alter table pt_co_tab_rng add partition co start(31) end(35) with (appendonly=true,orientation=column);

 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'pt_co_tab_rng_1_prt_heap', 'pt_co_tab_rng_1_prt_ao','pt_co_tab_rng_1_prt_co'));

--Split partition
 alter table pt_co_tab_rng split partition heap at (23) into (partition heap1,partition heap2);
 alter table pt_co_tab_rng split partition ao at (27) into (partition ao1,partition ao2);
 alter table pt_co_tab_rng split partition co  at (33) into (partition co1,partition co2);
 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in ( 'pt_co_tab_rng_1_prt_heap1' ,'pt_co_tab_rng_1_prt_heap2' ,'pt_co_tab_rng_1_prt_ao1', 'pt_co_tab_rng_1_prt_ao2', 'pt_co_tab_rng_1_prt_co1', 'pt_co_tab_rng_1_prt_co2'));

--Exchange
 -- Create candidate table
--start_ignore
drop table if exists heap_can cascade;
drop table if exists ao_can cascade;
drop table if exists co_can cascade;
--end_ignore
  create table heap_can(like pt_co_tab_rng);  
  create table ao_can(like pt_co_tab_rng) with (appendonly=true);   
  create table co_can(like pt_co_tab_rng)  with (appendonly=true,orientation=column);   

 alter table pt_co_tab_rng add partition newao start(36) end(40) with (appendonly= true);
 alter table pt_co_tab_rng add partition newheap start(40) end(45) with (appendonly= false);

 -- Exchange

 alter table pt_co_tab_rng exchange partition newheap with table ao_can;-- HEAP <=> AO
 alter table pt_co_tab_rng exchange partition newao with table co_can; -- AO <=> CO
 alter table pt_co_tab_rng exchange partition co1 with table heap_can; -- CO <=> HEAP

 \d+ ao_can 
 \d+ co_can
 \d+ heap_can

-- Create more index indexes
--start_ignore
drop index if exists co_rng_idx4 cascade;
drop index if exists co_rng_idx3 cascade;
--end_ignore

 create index co_rng_idx3 on pt_co_tab_rng(c,d) where a = 40 OR a = 50; -- multicol indx
 CREATE INDEX co_rng_idx4 ON pt_co_tab_rng ((b || ' ' || e)); --Expression

-- Add default partition
 alter table pt_co_tab_rng add default partition dft;

-- Split default partition
 alter table pt_co_tab_rng split default partition start(45) end(60) into (partition dft, partition two);
