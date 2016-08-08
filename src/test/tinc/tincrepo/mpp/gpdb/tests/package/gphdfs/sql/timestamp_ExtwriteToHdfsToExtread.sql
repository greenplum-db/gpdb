\echo --start_ignore
drop external table timestamp_heap;
drop external table timestamp_writehdfs;
drop external table timestamp_verification_mapreduce;
drop external table timestamp_verification_mapred;
drop external table timestamp_verification_blockcomp_mapreduce;
drop external table timestamp_verification_blockcomp_mapred;
drop external table timestamp_verification_recordcomp_mapreduce;
drop external table timestamp_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table timestamp_heap(datatype_timestamp varchar,xcount_timestamp bigint, col1_timestamp timestamp,col2_timestamp timestamp, col3_timestamp timestamp, nullcol_timestamp timestamp) location ('gpfdist://%localhost%:%gpfdistPort%/timestamp.txt')format 'TEXT';

create writable external table timestamp_writehdfs(like timestamp_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/timestamp')format 'custom' (formatter='gphdfs_export');
insert into timestamp_writehdfs select * from timestamp_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/timestamp /mapreduce/timestamp_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/timestamp /mapred/timestamp_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/timestamp /mapreduce/blockcomp/timestamp_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/timestamp /mapred/blockcomp/timestamp_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/timestamp /mapreduce/recordcomp/timestamp_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/timestamp /mapred/recordcomp/timestamp_gpdb/

create readable external table timestamp_verification_mapreduce(like timestamp_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/timestamp_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table timestamp_verification_mapred(like timestamp_heap) location ('gphdfs://%HADOOP_HOST%/mapred/timestamp_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table timestamp_verification_blockcomp_mapreduce(like timestamp_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/timestamp_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table timestamp_verification_blockcomp_mapred(like timestamp_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/timestamp_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table timestamp_verification_recordcomp_mapreduce(like timestamp_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/timestamp_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table timestamp_verification_recordcomp_mapred(like timestamp_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/timestamp_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from timestamp_verification_mapreduce except select * from timestamp_verification_mapred) union (select * from timestamp_verification_mapred except select * from timestamp_verification_mapreduce);
(select * from timestamp_verification_blockcomp_mapreduce except select * from timestamp_verification_blockcomp_mapred) union (select * from timestamp_verification_blockcomp_mapred except select * from timestamp_verification_blockcomp_mapreduce);
(select * from timestamp_verification_recordcomp_mapreduce except select * from timestamp_verification_recordcomp_mapred) union (select * from timestamp_verification_recordcomp_mapred except select * from timestamp_verification_recordcomp_mapreduce);
(select * from timestamp_verification_recordcomp_mapreduce except select * from timestamp_verification_blockcomp_mapreduce) union (select * from timestamp_verification_blockcomp_mapreduce except select * from timestamp_verification_recordcomp_mapreduce);
(select * from timestamp_verification_recordcomp_mapreduce except select * from timestamp_verification_mapreduce) union (select * from timestamp_verification_mapreduce except select * from timestamp_verification_recordcomp_mapreduce);
(select * from timestamp_verification_recordcomp_mapreduce except select * from timestamp_heap) union (select * from timestamp_heap except select * from timestamp_verification_recordcomp_mapreduce);



