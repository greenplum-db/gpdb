\echo --start_ignore
drop external table date_heap;
drop external table date_writehdfs;
drop external table date_verification_mapreduce;
drop external table date_verification_mapred;
drop external table date_verification_blockcomp_mapreduce;
drop external table date_verification_blockcomp_mapred;
drop external table date_verification_recordcomp_mapreduce;
drop external table date_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table date_heap(
datatype_date varchar,x_date bigint, col1_date date,col2_date date, col3_date date, col4_date date, col5_date date, col6_date date, col7_date date, nullcol_date date
) location ('gpfdist://%localhost%:%gpfdistPort%/date.txt')format 'TEXT';

create writable external table date_writehdfs(like date_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/date')format 'custom' (formatter='gphdfs_export');
insert into date_writehdfs select * from date_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/date /mapreduce/date_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/date /mapred/date_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/date /mapreduce/blockcomp/date_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/date /mapred/blockcomp/date_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/date /mapreduce/recordcomp/date_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/date /mapred/recordcomp/date_gpdb/

create readable external table date_verification_mapreduce(like date_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/date_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table date_verification_mapred(like date_heap) location ('gphdfs://%HADOOP_HOST%/mapred/date_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table date_verification_blockcomp_mapreduce(like date_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/date_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table date_verification_blockcomp_mapred(like date_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/date_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table date_verification_recordcomp_mapreduce(like date_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/date_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table date_verification_recordcomp_mapred(like date_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/date_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from date_verification_mapreduce except select * from date_verification_mapred) union (select * from date_verification_mapred except select * from date_verification_mapreduce);
(select * from date_verification_blockcomp_mapreduce except select * from date_verification_blockcomp_mapred) union (select * from date_verification_blockcomp_mapred except select * from date_verification_blockcomp_mapreduce);
(select * from date_verification_recordcomp_mapreduce except select * from date_verification_recordcomp_mapred) union (select * from date_verification_recordcomp_mapred except select * from date_verification_recordcomp_mapreduce);
(select * from date_verification_recordcomp_mapreduce except select * from date_verification_blockcomp_mapreduce) union (select * from date_verification_blockcomp_mapreduce except select * from date_verification_recordcomp_mapreduce);
(select * from date_verification_recordcomp_mapreduce except select * from date_verification_mapreduce) union (select * from date_verification_mapreduce except select * from date_verification_recordcomp_mapreduce);
(select * from date_verification_recordcomp_mapreduce except select * from date_heap) union (select * from date_heap except select * from date_verification_recordcomp_mapreduce);



