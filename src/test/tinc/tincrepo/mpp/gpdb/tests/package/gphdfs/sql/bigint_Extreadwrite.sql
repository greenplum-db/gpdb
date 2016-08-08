\echo -- start_ignore
drop  external table bigint_heap;
drop  external table bigint_writehdfs;
drop  external table bigint_readhdfs;
\echo -- end_ignore

create readable external table bigint_heap(datatype_bigint varchar,xcount_bigint bigint, max_bigint bigint, min_bigint bigint, x_bigint bigint, reverse_bigint bigint, increment_bigint bigint, nullcol_bigint bigint) location ('gpfdist://%localhost%:%gpfdistPort%/bigint.txt')format 'TEXT';
create writable external table bigint_writehdfs(like bigint_heap) location ('gphdfs://%HDFSaddr%/extwrite/bigint')format 'custom' (formatter='gphdfs_export');
create readable external table bigint_readhdfs(like bigint_heap) location ('gphdfs://%HDFSaddr%/extwrite/bigint') format 'custom' (formatter='gphdfs_import');

select count(*) from bigint_heap; 
insert into bigint_writehdfs select * from bigint_heap;
select count(*) from bigint_readhdfs;

(select * from bigint_heap except select * from bigint_readhdfs) union (select * from bigint_readhdfs except select * from bigint_heap);
