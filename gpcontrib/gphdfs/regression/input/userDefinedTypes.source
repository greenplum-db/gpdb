\echo --start_ignore
drop external table writeudt;
drop external table readudt;
drop type mytype CASCADE;
\echo --end_ignore


CREATE TYPE mytype AS (f1 int, f2 text);

create writable external table writeudt (id bigint, composit mytype) location ('gphdfs://%HADOOP_HOST%/extwrite/udt')FORMAT 'custom' (formatter='gphdfs_export');

insert into writeudt values (0, (1,'test') );
insert into writeudt values (1,  (2, '') );
insert into writeudt values (2, (3, null) );
insert into writeudt values (3, (12345, 'composittype'));

create readable external table readudt (id bigint, composit mytype) location ('gphdfs://%HADOOP_HOST%/extwrite/udt')FORMAT 'custom' (formatter='gphdfs_import');

select * from readudt order by id;

