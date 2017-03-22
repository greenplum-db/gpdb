--start_ignore 
drop schema if exists mpp17761 cascade;
create schema mpp17761;
--end_ignore
\echo '-- CASE#1 ( CO table )'
CREATE TABLE mpp17761.split_tab1
        (id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd')  WITH (appendonly=true, orientation=column) distributed randomly
 Partition by range(a1) (start(1)  end(5000) every(1000) ,
 COLUMN  a1 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=8192),
 COLUMN a5 ENCODING (compresstype=zlib,compresslevel=1, blocksize=8192),
 DEFAULT COLUMN ENCODING (compresstype=quicklz,compresslevel=1,blocksize=8192));

Alter table mpp17761.split_tab1 split partition FOR (RANK(2)) at(1050) into (partition splitc,partition splitd) ;

 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab1_1_prt_splitc','split_tab1_1_prt_splitd','split_tab1','split_tab1_1_prt_1'));

\echo '-- CASE#2 ( AO TABLE )'
CREATE TABLE mpp17761.split_tab6(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd')  
WITH (appendonly=true,compresstype=quicklz,compresslevel=1 ) distributed by (a5)
Partition by range(a1) 
(
    start(1)  end(5000) every(1000)
);

Alter table mpp17761.split_tab6 split partition FOR (RANK(2)) at(1050) into (partition splitc,partition splitd) ;

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab6_1_prt_splitc','split_tab6_1_prt_splitd','split_tab6','split_tab6_1_prt_1'));

\echo '-- CASE#3 ( HEAP TABLE )'

CREATE TABLE mpp17761.split_tab7(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd')  
WITH (appendonly = false ) distributed by (a5)
Partition by range(a1) 
(
    start(1)  end(5000) every(1000)
);

Alter table mpp17761.split_tab7 split partition FOR (RANK(2)) at(1050) into (partition splitc,partition splitd) ;

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab7_1_prt_splitc','split_tab7_1_prt_splitd','split_tab7','split_tab7_1_prt_1'));

\echo '-- CASE#4 ( PARENT: AO CHILD: CO )'
create table mpp17761.split_tab2( a int, b int) with (appendonly=true) partition by list(b) 
(
    partition a values (1,2,3,4) with (appendonly = true, orientation=column), 
    partition b values(5,6,7,8) with (appendonly = true, orientation=column)
);

alter table mpp17761.split_tab2 split partition for(2) at(2) into (partition one,partition two);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab2_1_prt_one','split_tab2_1_prt_two','split_tab2_1_prt_b','split_tab2'));

\echo '-- CASE#5 ( PARENT: AO CHILD: AO )'

create table mpp17761.split_tab8( a int, b int) with (appendonly=true) partition by list(b) 
(
    partition a values (1,2,3,4) with (appendonly = true, orientation=column), 
    partition b values(5,6,7,8) with (appendonly = true, orientation=column)
);

alter table mpp17761.split_tab8 add partition c values (11,12,13) with (appendonly=true);

alter table mpp17761.split_tab8 split partition c at (12) into (partition one,partition two);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where relname in  ( 'split_tab8_1_prt_one','split_tab8_1_prt_two','split_tab8_1_prt_b','split_tab8'));


\echo '-- CASE#6 ( PARENT: AO CHILD: HEAP )'
create table mpp17761.split_tab9( a int, b int) with (appendonly=true) partition by list(b) 
(
    partition a values (1,2,3,4) with (appendonly = false ), 
    partition b values(5,6,7,8) with (appendonly = false)
);

alter table mpp17761.split_tab9 split partition for(2) at(2) into (partition one,partition two);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab9_1_prt_one','split_tab9_1_prt_two','split_tab9_1_prt_b','split_tab9'));

\echo '-- CASE#7 ( PARENT: CO CHILD: CO )'

CREATE TABLE mpp17761.split_tab3 (a int, b text) with (appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1)
distributed by (a)
partition by range(a)
(
    start(1) end(20) every(5)
);

alter table mpp17761.split_tab3 add partition co start(31) end(35) with (appendonly=true,orientation=column);

alter table mpp17761.split_tab3 split partition co  at (33) into (partition co1,partition co2);

 select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab3_1_prt_1','split_tab3_1_prt_co1','split_tab3'));

\echo '-- CASE#8 ( PARENT: CO CHILD: AO )'
create table mpp17761.split_tab10( a int, b int) with (appendonly=true,orientation=column) partition by list(b) 
(
    partition a values (1,2,3,4) with (appendonly = true ), 
    partition b values(5,6,7,8) with (appendonly = true)
);

alter table mpp17761.split_tab10 split partition for(2) at(2) into (partition one,partition two);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab10_1_prt_one','split_tab10_1_prt_two','split_tab10_1_prt_b','split_tab10'));

\echo '-- CASE#9 ( PARENT: CO CHILD: HEAP )'

create table mpp17761.split_tab11( a int, b int) with (appendonly=true,orientation=column) partition by list(b) 
(
    partition a values (1,2,3,4) with (appendonly = false ), 
    partition b values(5,6,7,8) with (appendonly = false)
);

alter table mpp17761.split_tab11 split partition for(2) at(2) into (partition one,partition two);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab11_1_prt_one','split_tab11_1_prt_two','split_tab11_1_prt_b','split_tab11'));

\echo '-- CASE#10 ( PARENT: HEAP CHILD: CO )'

create table mpp17761.split_tab12( a int, b int) with (appendonly=false) partition by list(b) 
(
    partition a values (1,2,3,4) with (appendonly = true, orientation=column ), 
    partition b values(5,6,7,8) with (appendonly = false)
);

alter table mpp17761.split_tab12 split partition for(2) at(2) into (partition one,partition two);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab12_1_prt_one','split_tab12_1_prt_two','split_tab12_1_prt_b','split_tab12'));


\echo '-- CASE#11 ( PARENT: HEAP CHILD: AO )'

create table mpp17761.split_tab13( a int, b int) with (appendonly=false) partition by list(b) 
(
    partition a values (1,2,3,4) with (appendonly = true,compresstype=quicklz, compresslevel=1 ), 
    partition b values(5,6,7,8) with (appendonly = false)
);

alter table mpp17761.split_tab13 split partition for(2) at(2) into (partition one,partition two);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab13_1_prt_one','split_tab13_1_prt_two','split_tab13_1_prt_b','split_tab13'));

\echo '-- CASE#12 ( PARENT: HEAP CHILD: HEAP )'

CREATE TABLE mpp17761.split_tab14 (a int, b text) with (appendonly=false )
distributed by (a)
partition by range(a)
(
    start(1) end(20) every(5)
);

alter table mpp17761.split_tab14 add partition co start(31) end(35) with (appendonly= false);

alter table mpp17761.split_tab14 split partition co  at (33) into (partition co1,partition co2);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab14_1_prt_1','split_tab14_1_prt_co1','split_tab14_1_prt_co1','split_tab14'));


\echo '--CASE#13 (CO table with default partition )'

CREATE TABLE mpp17761.split_tab4( id SERIAL,a1 int,a2 text,a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',COLUMN a3 ENCODING (compresstype=zlib,compresslevel=1, blocksize=8192 )) with (appendonly=true, orientation=column)
partition by list(a3)
(
    partition a values(1.0,2.0,3.0,4.0) ,
    partition b values(5.0,6.0,7.0,8.0) ,
    default partition dft
);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab4_1_prt_a','split_tab4','split_tab4_1_prt_dft'));

alter table mpp17761.split_tab4 split default partition at (20.0) into  (partition dft,partition c);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab4_1_prt_a','split_tab4','split_tab4_1_prt_dft','split_tab4_1_prt_c'));

\echo '-- CASE#14 ( CO table , add heap partition , split)'
create table mpp17761.split_tab5( a int, b int, c char(5), d boolean default true) with (appendonly=true, orientation=column ) 
distributed randomly 
partition by list(b)
(
    partition a values (1,2,3,4) with (appendonly = true), 
    partition b values(5,6,7,8) with (appendonly = true),
    DEFAULT COLUMN ENCODING (compresstype=zlib,compresslevel=5)
);

alter table mpp17761.split_tab5 add partition c values (10,11,12,13) with (appendonly = false); 

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab5_1_prt_a','split_tab5_1_prt_b','split_tab5_1_prt_c','split_tab5'));

alter table mpp17761.split_tab5 split partition c at(12) into (partition d,partition e);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab5_1_prt_a','split_tab5_1_prt_b','split_tab5_1_prt_d','split_tab5'));


\echo '-- CASE#15 ( AO table , add Heap partition, split )'

create table mpp17761.split_tab15( a int, b int, c char(5), d boolean default true) with (appendonly=true) 
distributed randomly 
partition by range(b)
(start(1) end(25) every(10));

--add heap partition
alter table mpp17761.split_tab15 add partition c start(25) end(30) with (appendonly = false); 

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab15_1_prt_1','split_tab15_1_prt_2','split_tab15_1_prt_c','split_tab15'));

--split heap partition
alter table mpp17761.split_tab15 split partition c at(27) into (partition d,partition e);

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid in ( select oid from pg_class where   relname in  ( 'split_tab15_1_prt_1','split_tab15_1_prt_2','split_tab15','split_tab15_1_prt_e'));

