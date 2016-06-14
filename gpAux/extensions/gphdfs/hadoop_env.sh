export GP_JAVA_OPT='-Xmx1000m -XX:+DisplayVMOutputToStderr'
export PATH=$JAVA_HOME/bin:$PATH
export KRB5CCNAME=$GP_SEG_DATADIR/gpdb-gphdfs.krb5cc
JAVA=$JAVA_HOME/bin/java
CLASSPATH="$GPHOME"/"$GP_HADOOP_CONN_JARDIR"/$GP_HADOOP_CONN_VERSION.jar
export HADOOP_COMMON_HOME="${HADOOP_HOME}"
CLASSPATH=${HADOOP_HOME}/hadoop/etc/hadoop:${HADOOP_HOME}/etc/hadoop:${HADOOP_HOME}/conf:${CLASSPATH}

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

export GP_JAVA_OPT=$GP_JAVA_OPT
export CLASSPATH=$CLASSPATH
