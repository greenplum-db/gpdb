\echo --start_ignore
drop external table int_heap;
drop external table int_writehdfs;
drop external table int_verification_mapreduce;
drop external table int_verification_mapred;
drop external table int_verification_blockcomp_mapreduce;
drop external table int_verification_blockcomp_mapred;
drop external table int_verification_recordcomp_mapreduce;
drop external table int_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table int_heap(
datatype_int varchar, xcount_int bigint, max_int int, min_int int, x_int int, reverse_int int, increment_int int, nullcol_int int
) location ('gpfdist://%localhost%:%gpfdistPort%/int.txt')format 'TEXT';


create writable external table int_writehdfs(like int_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/int')format 'custom' (formatter='gphdfs_export');
insert into int_writehdfs select * from int_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/int /mapreduce/int_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/int /mapred/int_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/int /mapreduce/blockcomp/int_gpdb/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/int /mapred/blockcomp/int_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/int /mapreduce/recordcomp/int_gpdb/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/int /mapred/recordcomp/int_gpdb/

create readable external table int_verification_mapreduce(like int_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/int_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table int_verification_mapred(like int_heap) location ('gphdfs://%HADOOP_HOST%/mapred/int_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table int_verification_blockcomp_mapreduce(like int_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/blockcomp/int_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table int_verification_blockcomp_mapred(like int_heap) location ('gphdfs://%HADOOP_HOST%/mapred/blockcomp/int_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table int_verification_recordcomp_mapreduce(like int_heap) location ('gphdfs://%HADOOP_HOST%/mapreduce/recordcomp/int_gpdb/')format 'custom' (formatter='gphdfs_import');
create readable external table int_verification_recordcomp_mapred(like int_heap) location ('gphdfs://%HADOOP_HOST%/mapred/recordcomp/int_gpdb/')format 'custom' (formatter='gphdfs_import');

(select * from int_verification_mapreduce except select * from int_verification_mapred) union (select * from int_verification_mapred except select * from int_verification_mapreduce);
(select * from int_verification_blockcomp_mapreduce except select * from int_verification_blockcomp_mapred) union (select * from int_verification_blockcomp_mapred except select * from int_verification_blockcomp_mapreduce);
(select * from int_verification_recordcomp_mapreduce except select * from int_verification_recordcomp_mapred) union (select * from int_verification_recordcomp_mapred except select * from int_verification_recordcomp_mapreduce);
(select * from int_verification_recordcomp_mapreduce except select * from int_verification_blockcomp_mapreduce) union (select * from int_verification_blockcomp_mapreduce except select * from int_verification_recordcomp_mapreduce);
(select * from int_verification_recordcomp_mapreduce except select * from int_verification_mapreduce) union (select * from int_verification_mapreduce except select * from int_verification_recordcomp_mapreduce);
(select * from int_verification_recordcomp_mapreduce except select * from int_heap) union (select * from int_heap except select * from int_verification_recordcomp_mapreduce);



