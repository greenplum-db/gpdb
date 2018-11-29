\echo -- start_ignore
drop  external table float_heap;
drop  external table float_writehdfs;
drop  external table float_readhdfs;
\echo -- end_ignore

create readable external table float_heap(datatype_float varchar, x_float bigint, max_float float, min_float float, pi_float float, piX_float float, nullcol_float float) location ('gpfdist://%localhost%:%gpfdistPort%/float.txt')format 'TEXT';
create writable external table float_writehdfs(like float_heap) location ('gphdfs://%HDFSaddr%/extwrite/float')format 'custom' (formatter='gphdfs_export');
create readable external table float_readhdfs(like float_heap) location ('gphdfs://%HDFSaddr%/extwrite/float') format 'custom' (formatter='gphdfs_import');

select count(*) from float_heap;
insert into float_writehdfs select * from float_heap;
select count(*) from float_readhdfs;

(select * from float_heap except select * from float_readhdfs) union (select * from float_readhdfs except select * from float_heap);
