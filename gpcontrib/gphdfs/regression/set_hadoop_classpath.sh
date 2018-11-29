export HADOOP_COMMON_HOME="${HADOOP_HOME}"
CLASSPATH=${HADOOP_HOME}/hadoop/etc/hadoop:${HADOOP_HOME}/etc/hadoop:${HADOOP_HOME}/conf

# add libs to CLASSPATH
for f in $HADOOP_COMMON_HOME/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done

if [ -d "$HADOOP_COMMON_HOME/lib" ]; then
for f in $HADOOP_COMMON_HOME/lib/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_COMMON_HOME/hadoop/lib" ]; then
for f in $HADOOP_COMMON_HOME/hadoop/lib/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_COMMON_HOME/hadoop" ]; then
for f in $HADOOP_COMMON_HOME/hadoop/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_COMMON_HOME/hadoop-hdfs" ]; then
for f in $HADOOP_COMMON_HOME/hadoop-hdfs/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_COMMON_HOME/hadoop-mapreduce" ]; then
for f in $HADOOP_COMMON_HOME/hadoop-mapreduce/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_COMMON_HOME/hadoop-yarn" ]; then
for f in $HADOOP_COMMON_HOME/hadoop-yarn/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_COMMON_HOME/lib/jsp-2.1" ]; then
for f in $HADOOP_COMMON_HOME/lib/jsp-2.1/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
fi

#for hadoop 2.0
if [ -d "${HADOOP_HOME}/share/hadoop/common" ]; then
for f in ${HADOOP_HOME}/share/hadoop/common/*.jar; do
	  CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "${HADOOP_HOME}/share/hadoop/common/lib" ]; then
for f in ${HADOOP_HOME}/share/hadoop/common/lib/*.jar; do
	    CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_HOME/share/hadoop/hdfs" ]; then
for f in $HADOOP_HOME/share/hadoop/hdfs/*.jar; do
	CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_HOME/share/hadoop/mapreduce" ]; then
for f in $HADOOP_HOME/share/hadoop/mapreduce/*.jar; do
	CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_HOME/share/hadoop/yarn" ]; then
for f in $HADOOP_HOME/share/hadoop/yarn/*.jar; do
	CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_HOME/share/hadoop/httpfs/tomcat/lib" ]; then
for f in $HADOOP_HOME/share/hadoop/httpfs/tomcat/lib/*.jar; do
	  CLASSPATH=${CLASSPATH}:$f;
done
fi

#for hadoop 2.0 end

if [ -d "$HADOOP_COMMON_HOME/build/ivy/lib/Hadoop-Common/common" ]; then
for f in $HADOOP_COMMON_HOME/build/ivy/lib/Hadoop-Common/common/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_COMMON_HOME/build/ivy/lib/Hadoop-Hdfs/common" ]; then
for f in $HADOOP_COMMON_HOME/build/ivy/lib/Hadoop-Hdfs/common/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
fi

if [ -d "$HADOOP_COMMON_HOME/build/ivy/lib/Hadoop/common" ]; then
for f in $HADOOP_COMMON_HOME/build/ivy/lib/Hadoop/common/*.jar; do
  CLASSPATH=${CLASSPATH}:$f;
done
fi

export CLASSPATH=$CLASSPATH
