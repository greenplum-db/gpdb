-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Create table in a plpgsql function with the GUC set at database level, role level
Alter database dsp_db1 set gp_default_storage_options='appendonly=true, orientation=column, compresstype=rle_type, compresslevel=1';
Alter database dsp_db2 set gp_default_storage_options='appendonly=true,orientation=column';

Alter role dsp_role1 set gp_default_storage_options='appendonly=true, orientation=row,checksum=false, compresstype=quicklz';
Alter role dsp_role2 set gp_default_storage_options='appendonly=true, orientation=row, compresslevel=1';

select datname, datconfig from pg_database where datname in ('dsp_db1', 'dsp_db2');
select rolname, rolconfig from pg_roles where rolname in ('dsp_role1', 'dsp_role2');

\c dsp_db1 dsp_role1

show gp_default_storage_options;
create or replace function create_table() returns void as
$$
Begin
Create table dsp_pg_t1( i int, j int);
end
$$ language plpgsql;

create or replace function call_create_table() returns void as $$
Begin
perform create_table();
end
$$ language plpgsql;

select call_create_table();
\d+ dsp_pg_t1
select relkind, relstorage, reloptions from pg_class where relname='dsp_pg_t1';
Drop table if exists dsp_pg_t1;


Drop function create_table();
Drop function call_create_table();

-- Create table with compresstyp=rle_type when orientation=row to create exception

\c dsp_db2 dsp_role2
show gp_default_storage_options;
create or replace function create_table_with_exception() returns void as
$$
Begin
Create table dsp_pg_t2( i int, j int) with (compresstype=rle_type);
Exception when others then
raise notice 'rle_type is not supported for row oriented tables';
end
$$ language plpgsql;

create or replace function call_create_table_with_exception() returns void as $$
Begin
perform create_table_with_exception();
end
$$ language plpgsql;

select call_create_table_with_exception();

Drop function call_create_table_with_exception();
Drop function create_table_with_exception();
