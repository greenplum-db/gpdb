set allow_system_table_mods=DML;
update pg_index set indisunique='f' where indexrelid=(select oid from pg_class where relname='pg_compression_compname_index');
insert into pg_compression select * from pg_compression limit 1;
update pg_index set indisunique='t' where indexrelid=(select oid from pg_class where relname='pg_compression_compname_index');
select compname, count(*) from pg_compression group by compname having count(*) > 1;
