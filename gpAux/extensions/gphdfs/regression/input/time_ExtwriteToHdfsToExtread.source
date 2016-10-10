\echo --start_ignore
drop external table time_heap;
drop external table time_writehdfs;
drop external table time_verification_mapreduce;
drop external table time_verification_mapred;
drop external table time_verification_blockcomp_mapreduce;
drop external table time_verification_blockcomp_mapred;
drop external table time_verification_recordcomp_mapreduce;
drop external table time_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table time_heap(
datatype_time varchar,x_time bigint, col1_time time,col2_time time, col3_time time, col4_time time, col5_time time, col6_time time, col7_time time, col8_time time, col9_time time, nullcol_time time
) location ('gpfdist://%localhost%:%gpfdistPort%/time.txt')format 'TEXT';

create writable external table time_writehdfs(like time_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/time')format 'custom' (formatter='gphdfs_export');
insert into time_writehdfs select * from time_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/time /mapreduce/time_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/time /mapred/time_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/time /mapreduce/blockcomp/time_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/time /mapred/blockcomp/time_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/time /mapreduce/recordcomp/time_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/time /mapred/recordcomp/time_gpdb/

create readable external table time_verification_mapreduce(like time_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/time_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table time_verification_mapred(like time_heap) location ('gphdfs://%HADOOP_HOST%/mapred/time_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table time_verification_blockcomp_mapreduce(like time_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/time_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table time_verification_blockcomp_mapred(like time_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/time_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table time_verification_recordcomp_mapreduce(like time_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/time_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table time_verification_recordcomp_mapred(like time_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/time_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from time_verification_mapreduce except select * from time_verification_mapred) union (select * from time_verification_mapred except select * from time_verification_mapreduce);
(select * from time_verification_blockcomp_mapreduce except select * from time_verification_blockcomp_mapred) union (select * from time_verification_blockcomp_mapred except select * from time_verification_blockcomp_mapreduce);
(select * from time_verification_recordcomp_mapreduce except select * from time_verification_recordcomp_mapred) union (select * from time_verification_recordcomp_mapred except select * from time_verification_recordcomp_mapreduce);
(select * from time_verification_recordcomp_mapreduce except select * from time_verification_blockcomp_mapreduce) union (select * from time_verification_blockcomp_mapreduce except select * from time_verification_recordcomp_mapreduce);
(select * from time_verification_recordcomp_mapreduce except select * from time_verification_mapreduce) union (select * from time_verification_mapreduce except select * from time_verification_recordcomp_mapreduce);
(select * from time_verification_recordcomp_mapreduce except select * from time_heap) union (select * from time_heap except select * from time_verification_recordcomp_mapreduce);



