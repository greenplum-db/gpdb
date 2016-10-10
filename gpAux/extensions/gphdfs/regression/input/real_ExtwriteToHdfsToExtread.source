\echo --start_ignore
drop external table real_heap;
drop external table real_writehdfs;
drop external table real_verification_mapreduce;
drop external table real_verification_mapred;
drop external table real_verification_blockcomp_mapreduce;
drop external table real_verification_blockcomp_mapred;
drop external table real_verification_recordcomp_mapreduce;
drop external table real_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table real_heap(
datatype_real varchar,x_real bigint, max_real real, min_real real, pi_real real, piX_real real, nullcol_real real
) location ('gpfdist://%localhost%:%gpfdistPort%/real.txt')format 'TEXT';

create writable external table real_writehdfs(like real_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/real')format 'custom' (formatter='gphdfs_export');
insert into real_writehdfs select * from real_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/real /mapreduce/real_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/real /mapred/real_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/real /mapreduce/blockcomp/real_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/real /mapred/blockcomp/real_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/real /mapreduce/recordcomp/real_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/real /mapred/recordcomp/real_gpdb/

create readable external table real_verification_mapreduce(like real_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/real_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table real_verification_mapred(like real_heap) location ('gphdfs://%HADOOP_HOST%/mapred/real_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table real_verification_blockcomp_mapreduce(like real_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/real_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table real_verification_blockcomp_mapred(like real_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/real_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table real_verification_recordcomp_mapreduce(like real_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/real_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table real_verification_recordcomp_mapred(like real_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/real_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from real_verification_mapreduce except select * from real_verification_mapred) union (select * from real_verification_mapred except select * from real_verification_mapreduce);
(select * from real_verification_blockcomp_mapreduce except select * from real_verification_blockcomp_mapred) union (select * from real_verification_blockcomp_mapred except select * from real_verification_blockcomp_mapreduce);
(select * from real_verification_recordcomp_mapreduce except select * from real_verification_recordcomp_mapred) union (select * from real_verification_recordcomp_mapred except select * from real_verification_recordcomp_mapreduce);
(select * from real_verification_recordcomp_mapreduce except select * from real_verification_blockcomp_mapreduce) union (select * from real_verification_blockcomp_mapreduce except select * from real_verification_recordcomp_mapreduce);
(select * from real_verification_recordcomp_mapreduce except select * from real_verification_mapreduce) union (select * from real_verification_mapreduce except select * from real_verification_recordcomp_mapreduce);
(select * from real_verification_recordcomp_mapreduce except select * from real_heap) union (select * from real_heap except select * from real_verification_recordcomp_mapreduce);



