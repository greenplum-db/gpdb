#!/bin/bash -x

unset HADOOP_HOME
CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk.x86_64

if [ -z "$HADOOP_HOME_OVERRIDE" ]
then
	export HADOOP_HOME=/usr/hdp/2.3.2.0-2950
else
	export HADOOP_HOME=$HADOOP_HOME_OVERRIDE
fi

if [ -z "$GP_HADOOP_TARGET_VERSION" ]
then
	export GP_HADOOP_TARGET_VERSION=cdh4.1
fi

source $GPHOME/lib/hadoop/hadoop_env.sh; 

cd $CURDIR/legacy;
javac -cp .:$CLASSPATH:$GPHOME/lib/hadoop/${GP_HADOOP_TARGET_VERSION}-gnet-1.2.0.0.jar javaclasses/*.java
jar cf maptest.jar javaclasses/*.class
