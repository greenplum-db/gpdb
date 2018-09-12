\echo -- start_ignore
drop  external table time_heap;
drop  external table time_writehdfs;
drop  external table time_readhdfs;
\echo -- end_ignore

create readable external table time_heap(datatype_time varchar,x_time bigint, col1_time time,col2_time time, col3_time time, col4_time time, col5_time time, col6_time time, col7_time time, col8_time time, col9_time time, nullcol_time time) location ('gpfdist://%localhost%:%gpfdistPort%/time.txt')format 'TEXT';
create writable external table time_writehdfs(like time_heap) location ('gphdfs://%HDFSaddr%/extwrite/time')format 'custom' (formatter='gphdfs_export');
create readable external table time_readhdfs(like time_heap) location ('gphdfs://%HDFSaddr%/extwrite/time') format 'custom' (formatter='gphdfs_import');

select count(*) from time_heap;
insert into time_writehdfs select * from time_heap;
select count(*) from time_readhdfs;

(select * from time_heap except select * from time_readhdfs) union (select * from time_readhdfs except select * from time_heap);
