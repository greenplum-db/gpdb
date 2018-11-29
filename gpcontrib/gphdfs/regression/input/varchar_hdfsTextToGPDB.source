\echo --start_ignore
drop external table varchar_heap;
drop external table varchar_readhdfs_mapreduce;
drop external table varchar_readhdfs_mapred;
drop external table varchar_readhdfs_mapreduce_blockcomp;
drop external table varchar_readhdfs_mapreduce_recordcomp;
drop external table varchar_readhdfs_mapred_blockcomp;
drop external table varchar_readhdfs_mapred_recordcomp;
\echo --end_ignore

create readable external table varchar_heap(
datatype_varchar varchar,x_varchar bigint, col1_varchar varchar,col2_varchar varchar, nullcol_varchar varchar
) location ('gpfdist://%localhost%:%gpfdistPort%/varchar.txt')format 'TEXT'(ESCAPE  'OFF');

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_TextIn /plaintext/varchar.txt /mapreduce/varchar 
create readable external table varchar_readhdfs_mapreduce(like varchar_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/varchar')format 'custom' (formatter='gphdfs_import');

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_TextIn /plaintext/varchar.txt /mapred/varchar
create readable external table varchar_readhdfs_mapred(like varchar_readhdfs_mapreduce) location ('gphdfs://%HADOOP_HOST%/mapred/varchar')format 'custom' (formatter='gphdfs_import');

select * from varchar_readhdfs_mapreduce order by x_varchar limit 10;
select * from varchar_readhdfs_mapred order by x_varchar limit 10;

(select * from varchar_readhdfs_mapreduce except select * from varchar_readhdfs_mapred) union (select * from varchar_readhdfs_mapred except select * from varchar_readhdfs_mapreduce);

\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_TextIn /plaintext/varchar.txt /mapreduce/blockcomp/varchar
create readable external table varchar_readhdfs_mapreduce_blockcomp(like varchar_readhdfs_mapreduce) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/varchar')format 'custom' (formatter='gphdfs_import');

\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_TextIn /plaintext/varchar.txt /mapred/blockcomp/varchar
create readable external table varchar_readhdfs_mapred_blockcomp(like varchar_readhdfs_mapreduce) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/varchar')format 'custom' (formatter='gphdfs_import');


(select * from varchar_readhdfs_mapreduce_blockcomp except select * from varchar_readhdfs_mapred_blockcomp) union (select * from varchar_readhdfs_mapred_blockcomp except select * from varchar_readhdfs_mapreduce_blockcomp);

\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_TextIn /plaintext/varchar.txt /mapreduce/recordcomp/varchar
create readable external table varchar_readhdfs_mapreduce_recordcomp(like varchar_readhdfs_mapreduce) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/varchar')format 'custom' (formatter='gphdfs_import');

\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_TextIn /plaintext/varchar.txt /mapred/recordcomp/varchar
create readable external table varchar_readhdfs_mapred_recordcomp(like varchar_readhdfs_mapreduce) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/varchar')format 'custom' (formatter='gphdfs_import');


(select * from varchar_readhdfs_mapreduce_recordcomp except select * from varchar_readhdfs_mapred_recordcomp) union (select * from varchar_readhdfs_mapred_recordcomp except select * from varchar_readhdfs_mapreduce_recordcomp);

(select * from varchar_readhdfs_mapreduce_recordcomp except select * from varchar_readhdfs_mapred_blockcomp) union (select * from varchar_readhdfs_mapred_blockcomp except select * from varchar_readhdfs_mapreduce_recordcomp);

(select * from varchar_readhdfs_mapreduce except select * from varchar_readhdfs_mapred_recordcomp) union (select * from varchar_readhdfs_mapred_recordcomp except select * from varchar_readhdfs_mapreduce);

(select * from varchar_readhdfs_mapreduce except select * from varchar_heap) union (select * from varchar_heap except select * from varchar_readhdfs_mapreduce);


