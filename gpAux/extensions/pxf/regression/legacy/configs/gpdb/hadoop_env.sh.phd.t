export GP_JAVA_OPT=-Xmx100m
export JAVA_HOME=%JAVA_HOME%
export PATH=$JAVA_HOME/bin:$PATH
JAVA=$JAVA_HOME/bin/java
export GP_HADOOP_CONN_JARDIR=lib/hadoop
export GP_HADOOP_CONN_VERSION=%CONNECTOR%
CLASSPATH="$GPHOME"/"$GP_HADOOP_CONN_JARDIR"/$GP_HADOOP_CONN_VERSION.jar
export HADOOP_HOME=/usr/lib/gphd/hadoop
export HADOOP_COMMON_HOME=/usr/lib/gphd/hadoop
export YARN_HOME=/usr/lib/gphd/hadoop-yarn
export HADOOP_HDFS_HOME=/usr/lib/gphd/hadoop-hdfs
export HADOOP_MAPRED_HOME=/usr/lib/gphd/hadoop-mapreduce
CLASSPATH=${HADOOP_HOME}/etc/hadoop:${YARN_HOME}:${HADOOP_HDFS_HOME}:${HADOOP_COMMON_HOME}:${CLASSPATH}
# add libs to CLASSPATH
for f in $HADOOP_COMMON_HOME/lib/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
for f in $HADOOP_COMMON_HOME/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
for f in $HADOOP_COMMON_HOME/client/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
for f in $YARN_HOME/lib/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
for f in $YARN_HOME/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
for f in $HADOOP_HDFS_HOME/lib/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
for f in $HADOOP_HDFS_HOME/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
for f in $HADOOP_MAPRED_HOME/lib/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
for f in $HADOOP_MAPRED_HOME/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
# setup 'java.library.path' for native-hadoop code if necessary
JAVA_LIBRARY_PATH=''
if [ -d "${HADOOP_HOME}/build/native" -o -d "${HADOOP_HOME}/lib/native" ]; then
  JAVA_PLATFORM=`CLASSPATH=${CLASSPATH} ${JAVA} -Xmx32m ${HADOOP_JAVA_PLATFORM_OPTS} org.apache.hadoop.util.PlatformName | sed -e "s/ /_/g"`
  if [ -d "$HADOOP_HOME/build/native" ]; then
    JAVA_LIBRARY_PATH=${HADOOP_HOME}/build/native/${JAVA_PLATFORM}/lib
  fi
  if [ -d "${HADOOP_HOME}/lib/native" ]; then
    if [ "x$JAVA_LIBRARY_PATH" != "x" ]; then
      JAVA_LIBRARY_PATH=${JAVA_LIBRARY_PATH}:${HADOOP_HOME}/lib/native/${JAVA_PLATFORM}
    else
      JAVA_LIBRARY_PATH=${HADOOP_HOME}/lib/native/${JAVA_PLATFORM}
    fi
  fi
fi
export LD_LIBRARY_PATH="$JAVA_LIBRARY_PATH"
if [ "x$JAVA_LIBRARY_PATH" != "x" ]; then
  GP_JAVA_OPT="$GP_JAVA_OPT -Djava.library.path=$JAVA_LIBRARY_PATH"
fi
export GP_JAVA_OPT="$GP_JAVA_OPT -Djava.library.path=$HADOOP_HOME/lib/native/"
export CLASSPATH=$CLASSPATH