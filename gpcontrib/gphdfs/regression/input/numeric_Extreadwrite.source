\echo -- start_ignore
drop  external table numeric_heap;
drop  external table numeric_writehdfs;
drop  external table numeric_readhdfs;
\echo -- end_ignore

create readable external table numeric_heap(datatype_numeric varchar, xcount_numeric bigint,max_numeric numeric, min_numeric numeric, x_numeric numeric, reverse_numeric numeric, increment_numeric numeric, nullcol_numeric numeric) location ('gpfdist://%localhost%:%gpfdistPort%/numeric.txt')format 'TEXT';
create writable external table numeric_writehdfs(like numeric_heap) location ('gphdfs://%HDFSaddr%/extwrite/numeric')format 'custom' (formatter='gphdfs_export');
create readable external table numeric_readhdfs(like numeric_heap) location ('gphdfs://%HDFSaddr%/extwrite/numeric') format 'custom' (formatter='gphdfs_import');

select count(*) from numeric_heap;
insert into numeric_writehdfs select * from numeric_heap;
select count(*) from numeric_readhdfs;

(select * from numeric_heap except select * from numeric_readhdfs) union (select * from numeric_readhdfs except select * from numeric_heap);
