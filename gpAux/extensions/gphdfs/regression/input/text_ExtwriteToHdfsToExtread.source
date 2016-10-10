\echo --start_ignore
drop external table text_heap;
drop external table text_writehdfs;
drop external table text_verification_mapreduce;
drop external table text_verification_mapred;
drop external table text_verification_blockcomp_mapreduce;
drop external table text_verification_blockcomp_mapred;
drop external table text_verification_recordcomp_mapreduce;
drop external table text_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table text_heap(
datatype_text text,x_text bigint, col1_text text,col2_text text, nullcol_text text
) location ('gpfdist://%localhost%:%gpfdistPort%/text.txt')format 'TEXT';

create writable external table text_writehdfs(like text_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/text')format 'custom' (formatter='gphdfs_export');
insert into text_writehdfs select * from text_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/text /mapreduce/text_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/text /mapred/text_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/text /mapreduce/blockcomp/text_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/text /mapred/blockcomp/text_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/text /mapreduce/recordcomp/text_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/text /mapred/recordcomp/text_gpdb/

create readable external table text_verification_mapreduce(like text_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/text_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table text_verification_mapred(like text_heap) location ('gphdfs://%HADOOP_HOST%/mapred/text_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table text_verification_blockcomp_mapreduce(like text_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/text_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table text_verification_blockcomp_mapred(like text_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/text_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table text_verification_recordcomp_mapreduce(like text_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/text_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table text_verification_recordcomp_mapred(like text_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/text_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from text_verification_mapreduce except select * from text_verification_mapred) union (select * from text_verification_mapred except select * from text_verification_mapreduce);
(select * from text_verification_blockcomp_mapreduce except select * from text_verification_blockcomp_mapred) union (select * from text_verification_blockcomp_mapred except select * from text_verification_blockcomp_mapreduce);
(select * from text_verification_recordcomp_mapreduce except select * from text_verification_recordcomp_mapred) union (select * from text_verification_recordcomp_mapred except select * from text_verification_recordcomp_mapreduce);
(select * from text_verification_recordcomp_mapreduce except select * from text_verification_blockcomp_mapreduce) union (select * from text_verification_blockcomp_mapreduce except select * from text_verification_recordcomp_mapreduce);
(select * from text_verification_recordcomp_mapreduce except select * from text_verification_mapreduce) union (select * from text_verification_mapreduce except select * from text_verification_recordcomp_mapreduce);
(select * from text_verification_recordcomp_mapreduce except select * from text_heap) union (select * from text_heap except select * from text_verification_recordcomp_mapreduce);



