\echo --start_ignore
drop external table timestamp_heap;
drop external table timestamp_writehdfs;
drop external table timestamp_readhdfs_mapreduce;
drop external table timestamp_readhdfs_mapred;
drop external table timestamp_verification;
\echo --end_ignore

create readable external table timestamp_heap(datatype_timestamp varchar,xcount_timestamp bigint, col1_timestamp timestamp,col2_timestamp timestamp, col3_timestamp timestamp, nullcol_timestamp timestamp) location ('gpfdist://%localhost%:%gpfdistPort%/timestamp.txt')format 'TEXT';

create writable external table timestamp_writehdfs(like timestamp_heap) location ('gphdfs://%HADOOP_HOST%/extwrite/timestamp')format 'custom' (formatter='gphdfs_export');
insert into timestamp_writehdfs select * from timestamp_heap;

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDBIn /extwrite/timestamp /mapreduce/timestamp_text/ 
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDBIn /extwrite/timestamp /mapred/timestamp_text/

\!%HADOOP_HOME%/bin/hadoop fs -cat /mapreduce/timestamp_text/part* | sort --general-numeric-sort > /tmp/mapreduceOut.txt
\!%HADOOP_HOME%/bin/hadoop fs -cat /mapred/timestamp_text/part* | sort --general-numeric-sort > /tmp/mapredOut.txt

\!diff /tmp/mapreduceOut.txt /tmp/mapredOut.txt

\!%MYD%/parsefile.py /tmp/mapreduceOut.txt /tmp/timestamp_verification.txt

create readable external table timestamp_verification(like timestamp_heap) location ('gpfdist://%localhost%:%gpfdistPort%/timestamp.txt')format 'TEXT';

(select * from timestamp_verification except select * from timestamp_heap) union (select * from timestamp_heap except select * from timestamp_verification);

\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDBIn /extwrite/timestamp /mapreduce/blockcomp/timestamp_text/
\!%cmdstr% -DcompressionType=block javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDBIn /extwrite/timestamp /mapred/blockcomp/timestamp_text/

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_TEXT_INOUT /mapreduce/blockcomp/timestamp_text/ /mapreduce/timestamp/blockcompOut/timestamp_text/
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_TEXT_INOUT /mapred/blockcomp/timestamp_text/ /mapred/timestamp/blockcompOut/timestamp_text/

\!%HADOOP_HOME%/bin/hadoop fs -cat /mapreduce/timestamp/blockcompOut/timestamp_text/part* |sort --general-numeric-sort > /tmp/mapreduceBlockCompOut.txt
\!%HADOOP_HOME%/bin/hadoop fs -cat /mapred/timestamp/blockcompOut/timestamp_text/part* |sort --general-numeric-sort > /tmp/mapredBlockCompOut.txt

\!diff /tmp/mapreduceBlockCompOut.txt /tmp/mapredBlockCompOut.txt
\!diff /tmp/mapreduceOut.txt /tmp/mapreduceBlockCompOut.txt

\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_GPDBIn /extwrite/timestamp /mapreduce/recordcomp/timestamp_text/
\!%cmdstr% -DcompressionType=record javaclasses/TestHadoopIntegration mapred Mapred_mapper_GPDBIn /extwrite/timestamp /mapred/recordcomp/timestamp_text/

\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapreduce Mapreduce_mapper_TEXT_INOUT /mapreduce/recordcomp/timestamp_text/ /mapreduce/timestamp/recordcompOut/timestamp_text/
\!%cmdstr% -DcompressionType=none javaclasses/TestHadoopIntegration mapred Mapred_mapper_TEXT_INOUT /mapred/recordcomp/timestamp_text/ /mapred/timestamp/recordcompOut/timestamp_text/

\!%HADOOP_HOME%/bin/hadoop fs -cat /mapreduce/timestamp/recordcompOut/timestamp_text/part* |sort --general-numeric-sort > /tmp/mapreduceRecordCompOut.txt
\!%HADOOP_HOME%/bin/hadoop fs -cat /mapred/timestamp/recordcompOut/timestamp_text/part* |sort --general-numeric-sort > /tmp/mapredRecordCompOut.txt

\!diff /tmp/mapreduceRecordCompOut.txt /tmp/mapredRecordCompOut.txt
\!diff /tmp/mapreduceOut.txt /tmp/mapreduceRecordCompOut.txt


