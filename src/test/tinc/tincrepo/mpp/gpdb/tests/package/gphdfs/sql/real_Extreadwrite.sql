\echo -- start_ignore
drop  external table real_heap;
drop  external table real_writehdfs;
drop  external table real_readhdfs;
\echo -- end_ignore

create readable external table real_heap(datatype_real varchar,x_real bigint, max_real real, min_real real, pi_real real, piX_real real, nullcol_real real) location ('gpfdist://%localhost%:%gpfdistPort%/real.txt')format 'TEXT';
create writable external table real_writehdfs(like real_heap) location ('gphdfs://%HDFSaddr%/extwrite/real')format 'custom' (formatter='gphdfs_export');
create readable external table real_readhdfs(like real_heap) location ('gphdfs://%HDFSaddr%/extwrite/real') format 'custom' (formatter='gphdfs_import');

select count(*) from real_heap;
insert into real_writehdfs select * from real_heap;
select count(*) from real_readhdfs;

(select * from real_heap except select * from real_readhdfs) union (select * from real_readhdfs except select * from real_heap);
