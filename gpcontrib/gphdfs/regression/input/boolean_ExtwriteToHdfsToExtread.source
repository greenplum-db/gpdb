\echo --start_ignore
drop external table boolean_heap;
drop external table boolean_writehdfs;
drop external table boolean_verification_mapreduce;
drop external table boolean_verification_mapred;
drop external table boolean_verification_blockcomp_mapreduce;
drop external table boolean_verification_blockcomp_mapred;
drop external table boolean_verification_recordcomp_mapreduce;
drop external table boolean_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table boolean_heap(
datatype_boolean varchar, x_boolean bigint, col1_boolean boolean, nullcol_boolean boolean
) location ('gpfdist://%localhost%:%gpfdistPort%/boolean.txt')format 'TEXT';

create writable external table boolean_writehdfs(like boolean_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/boolean')format 'custom' (formatter='gphdfs_export');
insert into boolean_writehdfs select * from boolean_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/boolean /mapreduce/boolean_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/boolean /mapred/boolean_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/boolean /mapreduce/blockcomp/boolean_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/boolean /mapred/blockcomp/boolean_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/boolean /mapreduce/recordcomp/boolean_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/boolean /mapred/recordcomp/boolean_gpdb/

create readable external table boolean_verification_mapreduce(like boolean_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/boolean_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table boolean_verification_mapred(like boolean_heap) location ('gphdfs://%HADOOP_HOST%/mapred/boolean_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table boolean_verification_blockcomp_mapreduce(like boolean_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/boolean_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table boolean_verification_blockcomp_mapred(like boolean_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/boolean_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table boolean_verification_recordcomp_mapreduce(like boolean_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/boolean_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table boolean_verification_recordcomp_mapred(like boolean_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/boolean_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from boolean_verification_mapreduce except select * from boolean_verification_mapred) union (select * from boolean_verification_mapred except select * from boolean_verification_mapreduce);
(select * from boolean_verification_blockcomp_mapreduce except select * from boolean_verification_blockcomp_mapred) union (select * from boolean_verification_blockcomp_mapred except select * from boolean_verification_blockcomp_mapreduce);
(select * from boolean_verification_recordcomp_mapreduce except select * from boolean_verification_recordcomp_mapred) union (select * from boolean_verification_recordcomp_mapred except select * from boolean_verification_recordcomp_mapreduce);
(select * from boolean_verification_recordcomp_mapreduce except select * from boolean_verification_blockcomp_mapreduce) union (select * from boolean_verification_blockcomp_mapreduce except select * from boolean_verification_recordcomp_mapreduce);
(select * from boolean_verification_recordcomp_mapreduce except select * from boolean_verification_mapreduce) union (select * from boolean_verification_mapreduce except select * from boolean_verification_recordcomp_mapreduce);
(select * from boolean_verification_recordcomp_mapreduce except select * from boolean_heap) union (select * from boolean_heap except select * from boolean_verification_recordcomp_mapreduce);



