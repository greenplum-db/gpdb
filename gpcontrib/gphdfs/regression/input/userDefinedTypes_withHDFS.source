\echo --start_ignore
drop external table readgpfdist_compositeType;
drop external table composite_write_hdfs;
drop external table readudt_mapreduce;
drop external table readudt_mapred;
drop external table readudt;
drop type mytype CASCADE;
\echo --end_ignore

CREATE TYPE mytype AS (f1 int, f2 text);

create readable external table readgpfdist_compositeType (type varchar, id bigint, composite mytype) location ('gpfdist://%localhost%:%gpfdistPort%/compositeType.txt') format 'text';
create writable external table composite_write_hdfs(like readgpfdist_compositeType) location ('gphdfs://%HADOOP_HOST%/extwrite/composite') format 'custom' (formatter='gphdfs_export');
insert into composite_write_hdfs select * from readgpfdist_compositeType;

\echo --start_ignore
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/composite /mapreduce/composite_gpdb/
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/composite /mapred/composite_gpdb/
\echo --end_ignore

create readable external table readudt_mapreduce (like readgpfdist_compositeType) location ('gphdfs://%HADOOP_HOST%/mapreduce/composite_gpdb/')FORMAT 'custom' (formatter='gphdfs_import');
create readable external table readudt_mapred (like readgpfdist_compositeType) location ('gphdfs://%HADOOP_HOST%/mapred/composite_gpdb/')FORMAT 'custom' (formatter='gphdfs_import');

select * from readudt_mapreduce order by id;
select * from readudt_mapred order by id;

