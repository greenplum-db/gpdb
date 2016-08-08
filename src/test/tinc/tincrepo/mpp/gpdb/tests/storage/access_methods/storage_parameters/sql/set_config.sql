-- @product_version gpdb: [4.3.5.0-]
-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Set the guc value using function pg_catalog.set_config (is_local=true- within a transaction, false - for the entire session)

\c dsp_db1

-- is_local = false

select set_config('gp_default_storage_options', 'appendonly=true, orientation=column', false);

show gp_default_storage_options;

create table t1 (a int, b int) distributed by (a);
insert into t1 select i, i from generate_series(1, 10)i;
select * from t1 order by 1;
\d+ t1
drop table t1;

Begin;

select set_config('gp_default_storage_options', 'appendonly=true, compresstype=quicklz, checksum=false', true);

end;

show gp_default_storage_options;

-- Value of gp_default_storage_options outside of transactions

Begin;
set gp_default_storage_options='appendonly=true, orientation=column, compresslevel=5';
show gp_default_storage_options;

commit;

show gp_default_storage_options;

Begin;
set gp_default_storage_options='appendonly=true, orientation=column, compresslevel=3';
show gp_default_storage_options;

Rollback;
show gp_default_storage_options;
