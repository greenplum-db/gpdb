#!/bin/bash -x

unset HADOOP_HOME

export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk.x86_64
export HADOOP_HOME=/usr/hdp/2.3.2.0-2950
source /home/gpadmin/gpdb4391/greenplum-db/lib/hadoop/hadoop_env.sh;

cd /home/gpadmin/gpdb/gpAux/extensions/gphdfs/regression/legacy;
javac  -cp .:$CLASSPATH:/home/gpadmin/gpdb4391/greenplum-db-4.3.9.1/lib/hadoop/gphd-2.0.2-gnet-1.2.0.0.jar  javaclasses/*.java
jar cf maptest.jar javaclasses/*.class
