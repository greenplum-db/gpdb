\echo --start_ignore
drop external table bigint_heap;
drop external table bigint_writehdfs;
drop external table bigint_readhdfs_mapreduce;
drop external table bigint_readhdfs_mapred;
drop external table bigint_verification;
\echo --end_ignore

create readable external table bigint_heap(datatype_bigint varchar,xcount_bigint bigint, max_bigint bigint, min_bigint bigint, x_bigint bigint, reverse_bigint bigint, increment_bigint bigint, nullcol_bigint bigint) location ('gpfdist://%localhost%:%gpfdistPort%/bigint.txt')format 'TEXT';

create writable external table bigint_writehdfs(like bigint_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/bigint')format 'custom' (formatter='gphdfs_export');
insert into bigint_writehdfs select * from bigint_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDBIn /extwrite/bigint /mapreduce/bigint_text/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDBIn /extwrite/bigint /mapred/bigint_text/

\!%HADOOP_HOME%/bin/hadoop fs -cat /mapreduce/bigint_text/part* | sort --general-numeric-sort > /tmp/mapreduceOut.txt
\!%HADOOP_HOME%/bin/hadoop fs -cat /mapred/bigint_text/part* | sort --general-numeric-sort > /tmp/mapredOut.txt

\!diff /tmp/mapreduceOut.txt /tmp/mapredOut.txt

\!%MYD%/parsefile.py /tmp/mapreduceOut.txt /tmp/bigint_verification.txt

create readable external table bigint_verification(like bigint_heap) location ('gpfdist://%localhost%:%gpfdistPort%/bigint.txt')format 'TEXT';

(select * from bigint_verification except select * from bigint_heap) union (select * from bigint_heap except select * from bigint_verification);

\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDBIn /extwrite/bigint /mapreduce/blockcomp/bigint_text/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDBIn /extwrite/bigint /mapred/blockcomp/bigint_text/

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_TEXT_INOUT /mapreduce/blockcomp/bigint_text/ /mapreduce/bigint/blockcompOut/bigint_text/
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_TEXT_INOUT /mapred/blockcomp/bigint_text/ /mapred/bigint/blockcompOut/bigint_text/

\!%HADOOP_HOME%/bin/hadoop fs -cat /mapreduce/bigint/blockcompOut/bigint_text/part* |sort --general-numeric-sort > /tmp/mapreduceBlockCompOut.txt
\!%HADOOP_HOME%/bin/hadoop fs -cat /mapred/bigint/blockcompOut/bigint_text/part* |sort --general-numeric-sort > /tmp/mapredBlockCompOut.txt

\!diff /tmp/mapreduceBlockCompOut.txt /tmp/mapredBlockCompOut.txt
\!diff /tmp/mapreduceOut.txt /tmp/mapreduceBlockCompOut.txt

\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDBIn /extwrite/bigint /mapreduce/recordcomp/bigint_text/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDBIn /extwrite/bigint /mapred/recordcomp/bigint_text/

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_TEXT_INOUT /mapreduce/recordcomp/bigint_text/ /mapreduce/bigint/recordcompOut/bigint_text/
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_TEXT_INOUT /mapred/recordcomp/bigint_text/ /mapred/bigint/recordcompOut/bigint_text/

\!%HADOOP_HOME%/bin/hadoop fs -cat /mapreduce/bigint/recordcompOut/bigint_text/part* |sort --general-numeric-sort > /tmp/mapreduceRecordCompOut.txt
\!%HADOOP_HOME%/bin/hadoop fs -cat /mapred/bigint/recordcompOut/bigint_text/part* |sort --general-numeric-sort > /tmp/mapredRecordCompOut.txt

\!diff /tmp/mapreduceRecordCompOut.txt /tmp/mapredRecordCompOut.txt
\!diff /tmp/mapreduceOut.txt /tmp/mapreduceRecordCompOut.txt


