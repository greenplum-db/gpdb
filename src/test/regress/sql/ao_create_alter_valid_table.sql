--Tables with storage Parameters

CREATE TABLE table_storage_parameters (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
) WITH ( APPENDONLY=TRUE , COMPRESSLEVEL= 5 , FILLFACTOR= 50) DISTRIBUTED RANDOMLY;


-- create a table and load some data

create table aocomptest (a int, b int, c int, d int) WITH (appendonly=true,compresslevel=3);
-- start_ignore
\d aocomptest
-- end_ignore
insert into aocomptest select i, i%1, i%2, i%3 from generate_series(1,100)i;

-- vacuum analyze the table

vacuum analyze aocomptest;

-- rename the table

alter table aocomptest rename to new_test;
-- start_ignore
\d new_test
-- end_ignore
alter table new_test rename to aocomptest;
-- start_ignore
\d aocomptest
-- end_ignore


-- Negative Tests for AO Tables
 
create table foo_ao (a int) with (appendonly=true);
create table bar_ao (a int);

--invalid operations
-- start_ignore
delete from foo_ao;
-- end_ignore

-- try and trick by setting rules
create rule one as on insert to bar_ao do instead update foo_ao set a=1;
create rule two as on insert to bar_ao do instead delete from foo_ao where a=1;

-- create table with ao specification should fail
-- start_ignore
create table mpp2865_ao_syntax_test (a int, b int) with (compresslevel=5);
create table mpp2865_ao_syntax_test (a int, b int) with (blocksize=8192);
create table mpp2865_ao_sytax_text(a int, b int) with (appendonly=false,blocksize=8192);
-- end_ignore

-- AO tables with partitions

create table ggg_mpp2847 (a char(1), b char(2), d char(3)) with (appendonly=true) 
 distributed by (a) 
partition by LIST (b)
(
partition aa values ('a', 'b', 'c', 'd'),
partition bb values ('e', 'f', 'g')
);

insert into ggg_mpp2847 values ('x', 'a');
insert into ggg_mpp2847 values ('x', 'b');
insert into ggg_mpp2847 values ('x', 'c');
insert into ggg_mpp2847 values ('x', 'd');
insert into ggg_mpp2847 values ('x', 'e');
insert into ggg_mpp2847 values ('x', 'f');
insert into ggg_mpp2847 values ('x', 'g');
insert into ggg_mpp2847 values ('x', 'a');
insert into ggg_mpp2847 values ('x', 'b');
insert into ggg_mpp2847 values ('x', 'c');
insert into ggg_mpp2847 values ('x', 'd');
insert into ggg_mpp2847 values ('x', 'e');
insert into ggg_mpp2847 values ('x', 'f');
insert into ggg_mpp2847 values ('x', 'g');
select * from ggg_mpp2847;
-- drop table ggg_mpp2847;

-- AO check correct syntax

create table ao_syntax_test(a int, b int) WITH (appendonly=true);
select case when reloptions='{appendonly=true}' then 'Success' else 'Failure' end from pg_class where relname='ao_syntax_test';
drop table if exists ao_syntax_test;
create table ao_syntax_test(a int, b int) WITH (appendonly=false);
select case when reloptions='{appendonly=false}' then 'Success' else 'Failure' end from pg_class where relname='ao_syntax_test';
drop table if exists ao_syntax_test;
create table ao_syntax_test(a int, b int) WITH (appendonly=true,blocksize=8192);
select case when reloptions='{appendonly=true,blocksize=8192}' then 'Success' else 'Failure' end from pg_class where relname='ao_syntax_test';
drop table if exists ao_syntax_test;
create table ao_syntax_test(a int, b int) WITH (appendonly=true,blocksize=16384);
select case when reloptions='{appendonly=true,blocksize=16384}' then 'Success' else 'Failure' end from pg_class where relname='ao_syntax_test';
drop table if exists ao_syntax_test;
create table ao_syntax_test(a int, b int) WITH (appendonly=true,blocksize=24576);
select case when reloptions='{appendonly=true,blocksize=24576}' then 'Success' else 'Failure' end from pg_class where relname='ao_syntax_test';
drop table if exists ao_syntax_test;
create table ao_syntax_test(a int, b int) WITH (appendonly=true,blocksize=32768);
select case when reloptions='{appendonly=true,blocksize=32768}' then 'Success' else 'Failure' end from pg_class where relname='ao_syntax_test';
drop table if exists ao_syntax_test;
create table ao_syntax_test(a int, b int) WITH (appendonly=true,blocksize=40960);
select case when reloptions='{appendonly=true,blocksize=40960}' then 'Success' else 'Failure' end from pg_class where relname='ao_syntax_test';
drop table if exists ao_syntax_test;
create table ao_syntax_test(a int, b int) WITH (appendonly=true,blocksize=49152);
select case when reloptions='{appendonly=true,blocksize=49152}' then 'Success' else 'Failure' end from pg_class where relname='ao_syntax_test';

