\echo -- start_ignore
drop  external table smallint_heap;
drop  external table smallint_writehdfs;
drop  external table smallint_readhdfs;
\echo -- end_ignore

create readable external table smallint_heap(datatype_smallint varchar, xcount_smallint bigint, max_smallint smallint, min_smallint smallint, x_smallint smallint, reverse_smallint smallint, increment_smallint smallint, nullcol_smallint smallint) location ('gpfdist://%localhost%:%gpfdistPort%/smallint.txt')format 'TEXT';
create writable external table smallint_writehdfs(like smallint_heap) location ('gphdfs://%HDFSaddr%/extwrite/smallint')format 'custom' (formatter='gphdfs_export');
create readable external table smallint_readhdfs(like smallint_heap) location ('gphdfs://%HDFSaddr%/extwrite/smallint') format 'custom' (formatter='gphdfs_import');

select count(*) from smallint_heap;
insert into smallint_writehdfs select * from smallint_heap;
select count(*) from smallint_readhdfs;

(select * from smallint_heap except select * from smallint_readhdfs) union (select * from smallint_readhdfs except select * from smallint_heap);
