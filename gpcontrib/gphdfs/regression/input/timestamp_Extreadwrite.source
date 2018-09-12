\echo -- start_ignore
drop  external table timestamp_heap;
drop  external table timestamp_writehdfs;
drop  external table timestamp_readhdfs;
\echo -- end_ignore

create readable external table timestamp_heap(datatype_timestamp varchar,x_timestamp bigint, col1_timestamp timestamp,col2_timestamp timestamp, col3_timestamp timestamp, nullcol_timestamp timestamp) location ('gpfdist://%localhost%:%gpfdistPort%/timestamp.txt')format 'TEXT';
create writable external table timestamp_writehdfs(like timestamp_heap) location ('gphdfs://%HDFSaddr%/extwrite/timestamp')format 'custom' (formatter='gphdfs_export');
create readable external table timestamp_readhdfs(like timestamp_heap) location ('gphdfs://%HDFSaddr%/extwrite/timestamp') format 'custom' (formatter='gphdfs_import');

select count(*) from timestamp_heap;
insert into timestamp_writehdfs select * from timestamp_heap;
select count(*) from timestamp_readhdfs;

(select * from timestamp_heap except select * from timestamp_readhdfs) union (select * from timestamp_readhdfs except select * from timestamp_heap);
