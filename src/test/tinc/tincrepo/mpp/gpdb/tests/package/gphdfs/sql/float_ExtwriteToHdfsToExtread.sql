\echo --start_ignore
drop external table float_heap;
drop external table float_writehdfs;
drop external table float_verification_mapreduce;
drop external table float_verification_mapred;
drop external table float_verification_blockcomp_mapreduce;
drop external table float_verification_blockcomp_mapred;
drop external table float_verification_recordcomp_mapreduce;
drop external table float_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table float_heap(
datatype_float varchar, x_float bigint, max_float float, min_float float, pi_float float, piX_float float, nullcol_float float
) location ('gpfdist://%localhost%:%gpfdistPort%/float.txt')format 'TEXT';

create writable external table float_writehdfs(like float_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/float')format 'custom' (formatter='gphdfs_export');
insert into float_writehdfs select * from float_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/float /mapreduce/float_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/float /mapred/float_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/float /mapreduce/blockcomp/float_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/float /mapred/blockcomp/float_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/float /mapreduce/recordcomp/float_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/float /mapred/recordcomp/float_gpdb/

create readable external table float_verification_mapreduce(like float_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/float_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table float_verification_mapred(like float_heap) location ('gphdfs://%HADOOP_HOST%/mapred/float_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table float_verification_blockcomp_mapreduce(like float_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/float_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table float_verification_blockcomp_mapred(like float_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/float_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table float_verification_recordcomp_mapreduce(like float_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/float_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table float_verification_recordcomp_mapred(like float_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/float_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from float_verification_mapreduce except select * from float_verification_mapred) union (select * from float_verification_mapred except select * from float_verification_mapreduce);
(select * from float_verification_blockcomp_mapreduce except select * from float_verification_blockcomp_mapred) union (select * from float_verification_blockcomp_mapred except select * from float_verification_blockcomp_mapreduce);
(select * from float_verification_recordcomp_mapreduce except select * from float_verification_recordcomp_mapred) union (select * from float_verification_recordcomp_mapred except select * from float_verification_recordcomp_mapreduce);
(select * from float_verification_recordcomp_mapreduce except select * from float_verification_blockcomp_mapreduce) union (select * from float_verification_blockcomp_mapreduce except select * from float_verification_recordcomp_mapreduce);
(select * from float_verification_recordcomp_mapreduce except select * from float_verification_mapreduce) union (select * from float_verification_mapreduce except select * from float_verification_recordcomp_mapreduce);
(select * from float_verification_recordcomp_mapreduce except select * from float_heap) union (select * from float_heap except select * from float_verification_recordcomp_mapreduce);



