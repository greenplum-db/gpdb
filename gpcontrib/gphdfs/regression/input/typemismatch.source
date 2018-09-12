\echo --start_ignore
drop external table bigint_heap;
drop external table bigint_writehdfs;
drop external table bigint_verification_mapreduce;
drop external table bigint_verification_mapred;
drop external table bigint_verification_blockcomp_mapreduce;
drop external table bigint_verification_blockcomp_mapred;
drop external table bigint_verification_recordcomp_mapreduce;
drop external table bigint_verification_recordcomp_mapred;
\echo --end_ignore

create readable external table bigint_heap(
datatype_bigint varchar,xcount_bigint bigint, max_bigint bigint, min_bigint bigint, x_bigint bigint, reverse_bigint bigint, increment_bigint bigint, nullcol_bigint bigint
--datatype_bigint varchar,xcount_bigint bigint, max_bigint bigint, min_bigint bigint, x_bigint bigint, reverse_bigint bigint, increment_bigint bigint
) location ('gpfdist://%localhost%:%gpfdistPort%/bigint_text.txt')format 'TEXT';

create writable external table bigint_writehdfs(like bigint_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/bigint')format 'custom' (formatter='gphdfs_export');
insert into bigint_writehdfs select * from bigint_heap;
\echo --start_ignore
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDB_INOUT /extwrite/bigint /mapreduce/bigint_gpdb/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDB_INOUT /extwrite/bigint /mapred/bigint_gpdb/
\echo --end_ignore

\!grep -m 1 "Mapreduce exception: Cannot get" %MYD%/output/typemismatch.out
\!grep -m 1 "Mapred exception: Cannot get" %MYD%/output/typemismatch.out
