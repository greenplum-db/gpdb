drop table if exists ao_t1;
drop table if exists co_t1;

create table ao_t1 with(appendonly=true) as select * from generate_series(1, 100) as a;
create table co_t1 with(appendonly=true, orientation=column) as select * from generate_series(1, 100) as a;

create index ao_i1 on ao_t1(a);
create index co_i1 on co_t1(a);

--Verify that the indexes have unique and primary key constraints on them
select indisunique, indisprimary from pg_index where indexrelid = (select oid from pg_class where relname = 'pg_aovisimap_' || (select oid from pg_class where relname = 'ao_t1') || '_index');
select indisunique, indisprimary from pg_index where indexrelid = (select oid from pg_class where relname = 'pg_aoblkdir_' || (select oid from pg_class where relname = 'ao_t1') || '_index');
select indisunique, indisprimary from pg_index where indexrelid = (select oid from pg_class where relname = 'pg_aovisimap_' || (select oid from pg_class where relname = 'co_t1') || '_index');
select indisunique, indisprimary from pg_index where indexrelid = (select oid from pg_class where relname = 'pg_aoblkdir_' || (select oid from pg_class where relname = 'co_t1') || '_index');
