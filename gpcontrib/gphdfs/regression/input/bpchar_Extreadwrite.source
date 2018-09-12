\echo -- start_ignore
drop  external table bpchar_heap;
drop  external table bpchar_writehdfs;
drop  external table bpchar_readhdfs;
\echo -- end_ignore

create readable external table bpchar_heap(datatype_bpchar bpchar,x_bpchar bigint, col1_bpchar bpchar,col2_bpchar bpchar, nullcol_bpchar bpchar) location ('gpfdist://%localhost%:%gpfdistPort%/bpchar.txt')format 'TEXT';
create writable external table bpchar_writehdfs(like bpchar_heap) location ('gphdfs://%HDFSaddr%/extwrite/bpchar')format 'custom' (formatter='gphdfs_export');
create readable external table bpchar_readhdfs(like bpchar_heap) location ('gphdfs://%HDFSaddr%/extwrite/bpchar') format 'custom' (formatter='gphdfs_import');

select count(*) from bpchar_heap;
insert into bpchar_writehdfs select * from bpchar_heap;
select count(*) from bpchar_readhdfs;

(select * from bpchar_heap except select * from bpchar_readhdfs) union (select * from bpchar_readhdfs except select * from bpchar_heap);
