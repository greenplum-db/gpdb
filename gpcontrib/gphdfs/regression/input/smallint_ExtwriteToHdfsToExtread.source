\echo --start_ignore
drop external table smallint_heap;
drop external table smallint_writehdfs;
drop external table smallint_verification_mapreduce;
drop external table smallint_verification_mapred;
drop external table smallint_verification_blockcomp_mapreduce;
drop external table smallint_verification_blockcomp_mapred;
drop external table smallint_verification_recordcomp_mapreduce;
drop external table smallint_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table smallint_heap(
datatype_smallint varchar, xcount_smallint bigint, max_smallint smallint, min_smallint smallint, x_smallint smallint, reverse_smallint smallint, increment_smallint smallint, nullcol_smallint smallint
) location ('gpfdist://%localhost%:%gpfdistPort%/smallint.txt')format 'TEXT';


create writable external table smallint_writehdfs(like smallint_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/smallint')format 'custom' (formatter='gphdfs_export');
insert into smallint_writehdfs select * from smallint_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/smallint /mapreduce/smallint_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/smallint /mapred/smallint_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/smallint /mapreduce/blockcomp/smallint_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/smallint /mapred/blockcomp/smallint_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/smallint /mapreduce/recordcomp/smallint_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/smallint /mapred/recordcomp/smallint_gpdb/

create readable external table smallint_verification_mapreduce(like smallint_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/smallint_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table smallint_verification_mapred(like smallint_heap) location ('gphdfs://%HADOOP_HOST%/mapred/smallint_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table smallint_verification_blockcomp_mapreduce(like smallint_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/smallint_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table smallint_verification_blockcomp_mapred(like smallint_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/smallint_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table smallint_verification_recordcomp_mapreduce(like smallint_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/smallint_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table smallint_verification_recordcomp_mapred(like smallint_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/smallint_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from smallint_verification_mapreduce except select * from smallint_verification_mapred) union (select * from smallint_verification_mapred except select * from smallint_verification_mapreduce);
(select * from smallint_verification_blockcomp_mapreduce except select * from smallint_verification_blockcomp_mapred) union (select * from smallint_verification_blockcomp_mapred except select * from smallint_verification_blockcomp_mapreduce);
(select * from smallint_verification_recordcomp_mapreduce except select * from smallint_verification_recordcomp_mapred) union (select * from smallint_verification_recordcomp_mapred except select * from smallint_verification_recordcomp_mapreduce);
(select * from smallint_verification_recordcomp_mapreduce except select * from smallint_verification_blockcomp_mapreduce) union (select * from smallint_verification_blockcomp_mapreduce except select * from smallint_verification_recordcomp_mapreduce);
(select * from smallint_verification_recordcomp_mapreduce except select * from smallint_verification_mapreduce) union (select * from smallint_verification_mapreduce except select * from smallint_verification_recordcomp_mapreduce);
(select * from smallint_verification_recordcomp_mapreduce except select * from smallint_heap) union (select * from smallint_heap except select * from smallint_verification_recordcomp_mapreduce);



