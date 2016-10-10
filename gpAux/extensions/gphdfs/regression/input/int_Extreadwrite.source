\echo -- start_ignore
drop  external table int_heap;
drop  external table int_writehdfs;
drop  external table int_readhdfs;
\echo -- end_ignore

create readable external table int_heap(datatype_int varchar,xcount_int bigint, max_int int, min_int int, x_int int, reverse_int int, increment_int int, nullcol_int int) location ('gpfdist://%localhost%:%gpfdistPort%/int.txt')format 'TEXT';
create writable external table int_writehdfs(like int_heap) location ('gphdfs://%HDFSaddr%/extwrite/int')format 'custom' (formatter='gphdfs_export');
create readable external table int_readhdfs(like int_heap) location ('gphdfs://%HDFSaddr%/extwrite/int') format 'custom' (formatter='gphdfs_import');

select count(*) from int_heap;
insert into int_writehdfs select * from int_heap;
select count(*) from int_readhdfs;

(select * from int_heap except select * from int_readhdfs) union (select * from int_readhdfs except select * from int_heap);
