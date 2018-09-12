\echo -- start_ignore
drop  external table text_heap;
drop  external table text_writehdfs;
drop  external table text_readhdfs;
\echo -- end_ignore

create readable external table text_heap(datatype_text text,x_text bigint, col1_text text,col2_text text, nullcol_text text) location ('gpfdist://%localhost%:%gpfdistPort%/text.txt')format 'TEXT';
create writable external table text_writehdfs(like text_heap) location ('gphdfs://%HDFSaddr%/extwrite/text')format 'custom' (formatter='gphdfs_export');
create readable external table text_readhdfs(like text_heap) location ('gphdfs://%HDFSaddr%/extwrite/text') format 'custom' (formatter='gphdfs_import');

select count(*) from text_heap;
insert into text_writehdfs select * from text_heap;
select count(*) from text_readhdfs;

(select * from text_heap except select * from text_readhdfs) union (select * from text_readhdfs except select * from text_heap);
