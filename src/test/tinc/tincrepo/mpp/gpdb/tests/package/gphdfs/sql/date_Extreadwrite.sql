\echo -- start_ignore
drop  external table date_heap;
drop  external table date_writehdfs;
drop  external table date_readhdfs;
\echo -- end_ignore

create readable external table date_heap(datatype_date varchar,x_date bigint, col1_date date,col2_date date, col3_date date, col4_date date, col5_date date, col6_date date, col7_date date, nullcol_date date) location ('gpfdist://%localhost%:%gpfdistPort%/date.txt')format 'TEXT';
create writable external table date_writehdfs(like date_heap) location ('gphdfs://%HDFSaddr%/extwrite/date')format 'custom' (formatter='gphdfs_export');
create readable external table date_readhdfs(like date_heap) location ('gphdfs://%HDFSaddr%/extwrite/date') format 'custom' (formatter='gphdfs_import');

select count(*) from date_heap;
insert into date_writehdfs select * from date_heap;
select count(*) from date_readhdfs;

(select * from date_heap except select * from date_readhdfs) union (select * from date_readhdfs except select * from date_heap);
