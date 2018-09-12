\echo -- start_ignore
drop  external table write_mpp16780;
drop  external table read_mpp16780;
\echo -- end_ignore

create writable external table write_mpp16780 (a int, b int) location ('gphdfs://%HDFSaddr%/extwrite/int') format 'custom' (formatter='gphdfs_export');
insert into write_mpp16780 values (generate_series(1,1000000),1);
insert into write_mpp16780 values (generate_series(1,1000000),1);
insert into write_mpp16780 values (generate_series(1,1000000),1);
create readable external table read_mpp16780 (a int, b int) location ('gphdfs://%HDFSaddr%/extwrite/int') format 'custom' (formatter='gphdfs_import');
select * from read_mpp16780 where a = 1 limit 1;
