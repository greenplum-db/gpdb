\echo -- start_ignore
drop  external table varchar_heap;
drop  external table varchar_writehdfs;
drop  external table varchar_readhdfs;
\echo -- end_ignore

create readable external table varchar_heap(datatype_varchar varchar,x_varchar bigint, col1_varchar varchar,col2_varchar varchar, nullcol_varchar varchar) location ('gpfdist://%localhost%:%gpfdistPort%/varchar.txt')format 'TEXT';
create writable external table varchar_writehdfs(like varchar_heap) location ('gphdfs://%HDFSaddr%/extwrite/varchar')format 'custom' (formatter='gphdfs_export');
create readable external table varchar_readhdfs(like varchar_heap) location ('gphdfs://%HDFSaddr%/extwrite/varchar') format 'custom' (formatter='gphdfs_import');

select count(*) from varchar_heap;
insert into varchar_writehdfs select * from varchar_heap;
select count(*) from varchar_readhdfs;

(select * from varchar_heap except select * from varchar_readhdfs) union (select * from varchar_readhdfs except select * from varchar_heap);
