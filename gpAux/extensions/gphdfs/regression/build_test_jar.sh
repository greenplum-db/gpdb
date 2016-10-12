#!/bin/bash -x

unset HADOOP_HOME
CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk.x86_64

if [ -z "$1" ]
then
	export HADOOP_HOME=/usr/hdp/2.3.2.0-2950
else
	export HADOOP_HOME=$1
fi

source $GPHOME/lib/hadoop/hadoop_env.sh; 

cd $CURDIR/legacy;
javac -cp .:$CLASSPATH:$GPHOME/lib/hadoop/gphd-2.0.2-gnet-1.2.0.0.jar javaclasses/*.java
jar cf maptest.jar javaclasses/*.class
