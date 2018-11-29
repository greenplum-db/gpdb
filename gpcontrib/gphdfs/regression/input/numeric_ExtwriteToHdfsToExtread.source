\echo --start_ignore
drop external table numeric_heap;
drop external table numeric_writehdfs;
drop external table numeric_verification_mapreduce;
drop external table numeric_verification_mapred;
drop external table numeric_verification_blockcomp_mapreduce;
drop external table numeric_verification_blockcomp_mapred;
drop external table numeric_verification_recordcomp_mapreduce;
drop external table numeric_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table numeric_heap(
datatype_numeric varchar,xcount_numeric bigint, max_numeric numeric, min_numeric numeric, x_numeric numeric, reverse_numeric numeric, increment_numeric numeric, nullcol_numeric numeric
) location ('gpfdist://%localhost%:%gpfdistPort%/numeric.txt')format 'TEXT';

create writable external table numeric_writehdfs(like numeric_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/numeric')format 'custom' (formatter='gphdfs_export');
insert into numeric_writehdfs select * from numeric_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/numeric /mapreduce/numeric_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/numeric /mapred/numeric_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/numeric /mapreduce/blockcomp/numeric_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/numeric /mapred/blockcomp/numeric_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/numeric /mapreduce/recordcomp/numeric_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/numeric /mapred/recordcomp/numeric_gpdb/

create readable external table numeric_verification_mapreduce(like numeric_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/numeric_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table numeric_verification_mapred(like numeric_heap) location ('gphdfs://%HADOOP_HOST%/mapred/numeric_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table numeric_verification_blockcomp_mapreduce(like numeric_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/numeric_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table numeric_verification_blockcomp_mapred(like numeric_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/numeric_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table numeric_verification_recordcomp_mapreduce(like numeric_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/numeric_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table numeric_verification_recordcomp_mapred(like numeric_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/numeric_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from numeric_verification_mapreduce except select * from numeric_verification_mapred) union (select * from numeric_verification_mapred except select * from numeric_verification_mapreduce);
(select * from numeric_verification_blockcomp_mapreduce except select * from numeric_verification_blockcomp_mapred) union (select * from numeric_verification_blockcomp_mapred except select * from numeric_verification_blockcomp_mapreduce);
(select * from numeric_verification_recordcomp_mapreduce except select * from numeric_verification_recordcomp_mapred) union (select * from numeric_verification_recordcomp_mapred except select * from numeric_verification_recordcomp_mapreduce);
(select * from numeric_verification_recordcomp_mapreduce except select * from numeric_verification_blockcomp_mapreduce) union (select * from numeric_verification_blockcomp_mapreduce except select * from numeric_verification_recordcomp_mapreduce);
(select * from numeric_verification_recordcomp_mapreduce except select * from numeric_verification_mapreduce) union (select * from numeric_verification_mapreduce except select * from numeric_verification_recordcomp_mapreduce);
(select * from numeric_verification_recordcomp_mapreduce except select * from numeric_heap) union (select * from numeric_heap except select * from numeric_verification_recordcomp_mapreduce);



