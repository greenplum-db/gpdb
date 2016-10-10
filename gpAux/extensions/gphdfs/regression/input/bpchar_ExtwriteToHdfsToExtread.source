\echo --start_ignore
drop external table bpchar_heap;
drop external table bpchar_writehdfs;
drop external table bpchar_verification_mapreduce;
drop external table bpchar_verification_mapred;
drop external table bpchar_verification_blockcomp_mapreduce;
drop external table bpchar_verification_blockcomp_mapred;
drop external table bpchar_verification_recordcomp_mapreduce;
drop external table bpchar_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table bpchar_heap(
datatype_bpchar bpchar,x_bpchar bigint, col1_bpchar bpchar,col2_bpchar bpchar, nullcol_bpchar bpchar
) location ('gpfdist://%localhost%:%gpfdistPort%/bpchar.txt')format 'TEXT';

create writable external table bpchar_writehdfs(like bpchar_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/bpchar')format 'custom' (formatter='gphdfs_export');
insert into bpchar_writehdfs select * from bpchar_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/bpchar /mapreduce/bpchar_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/bpchar /mapred/bpchar_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/bpchar /mapreduce/blockcomp/bpchar_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/bpchar /mapred/blockcomp/bpchar_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/bpchar /mapreduce/recordcomp/bpchar_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/bpchar /mapred/recordcomp/bpchar_gpdb/

create readable external table bpchar_verification_mapreduce(like bpchar_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/bpchar_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table bpchar_verification_mapred(like bpchar_heap) location ('gphdfs://%HADOOP_HOST%/mapred/bpchar_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table bpchar_verification_blockcomp_mapreduce(like bpchar_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/bpchar_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table bpchar_verification_blockcomp_mapred(like bpchar_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/bpchar_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table bpchar_verification_recordcomp_mapreduce(like bpchar_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/bpchar_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table bpchar_verification_recordcomp_mapred(like bpchar_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/bpchar_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from bpchar_verification_mapreduce except select * from bpchar_verification_mapred) union (select * from bpchar_verification_mapred except select * from bpchar_verification_mapreduce);
(select * from bpchar_verification_blockcomp_mapreduce except select * from bpchar_verification_blockcomp_mapred) union (select * from bpchar_verification_blockcomp_mapred except select * from bpchar_verification_blockcomp_mapreduce);
(select * from bpchar_verification_recordcomp_mapreduce except select * from bpchar_verification_recordcomp_mapred) union (select * from bpchar_verification_recordcomp_mapred except select * from bpchar_verification_recordcomp_mapreduce);
(select * from bpchar_verification_recordcomp_mapreduce except select * from bpchar_verification_blockcomp_mapreduce) union (select * from bpchar_verification_blockcomp_mapreduce except select * from bpchar_verification_recordcomp_mapreduce);
(select * from bpchar_verification_recordcomp_mapreduce except select * from bpchar_verification_mapreduce) union (select * from bpchar_verification_mapreduce except select * from bpchar_verification_recordcomp_mapreduce);
(select * from bpchar_verification_recordcomp_mapreduce except select * from bpchar_heap) union (select * from bpchar_heap except select * from bpchar_verification_recordcomp_mapreduce);



