#!/bin/bash -x

set -e
if [ -z ${GPHOME+x} ]; then echo "GPHOME is unset";exit 1 ; fi
export GP_HADOOP_TARGET_VERSION=cdh4.1
export HADOOP_HOST=10.152.10.234
export GPDB_HADOOP_HOME=/usr/hdp/2.3.2.0-2950
export HADOOP_HOME=$GPDB_HADOOP_HOME/hadoop
export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk.x86_64/jre
export HADOOP_PORT=8020

CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PGREGRESS=$GPHOME/lib/postgresql/pgxs/src/test/regress/pg_regress
HADOOPCMD=$HADOOP_HOME/bin/hadoop

rm -rf $CURDIR/source_replaced/
mkdir -p $CURDIR/source_replaced/input
mkdir -p $CURDIR/source_replaced/output

cp $CURDIR/input/*.source $CURDIR/source_replaced/input/
cp $CURDIR/output/*.source $CURDIR/source_replaced/output/

for f in $(ls $CURDIR/source_replaced/input);do
  echo -e  "--start_ignore\n\!%HADOOP_HOME%/bin/hadoop fs -rm -r /mapreduce/*\n\!%HADOOP_HOME%/bin/hadoop fs -rm -r /mapred/*\n--end_ignore" >> "$CURDIR/source_replaced/input/$f"
  sed -i "s|gpfdist://%localhost%:%gpfdistPort%|gphdfs://${HADOOP_HOST}:${HADOOP_PORT}/plaintext|g" "$CURDIR/source_replaced/input/$f"
  sed -i "s|%cmdstr%|${CURDIR}/runcmd|g" "$CURDIR/source_replaced/input/$f"
  sed -i "s|%HADOOP_HOST%|${HADOOP_HOST}:${HADOOP_PORT}|g" "$CURDIR/source_replaced/input/$f"
  sed -i "s|%HDFSaddr%|${HADOOP_HOST}:${HADOOP_PORT}|g" "$CURDIR/source_replaced/input/$f"
  sed -i "s|%HADOOP_HOME%|${HADOOP_HOME}|g" "$CURDIR/source_replaced/input/$f"
  sed -i "s|%MYD%|${CURDIR}/source_replaced/input|g" "$CURDIR/source_replaced/input/$f"
  sed -i "s|%HADOOP_FS%|${HADOOPCMD}|g" "$CURDIR/source_replaced/input/$f"
done

cp $CURDIR/input/parsefile.py  $CURDIR/source_replaced/input/

cp "${CURDIR}/runcmd_tpl" "${CURDIR}/runcmd"
sed -i "s|%GPHOME%|${GPHOME}|g" "${CURDIR}/runcmd"
sed -i "s|%HADOOP_HOME%|${HADOOP_HOME}|g" "${CURDIR}/runcmd"
sed -i "s|%JAVA_HOME%|${JAVA_HOME}|g" "${CURDIR}/runcmd"
sed -i "s|%WORKDIR%|${CURDIR}|g" "${CURDIR}/runcmd"
sed -i "s|%HADOOP_HOST%|${HADOOP_HOST}|g" "${CURDIR}/runcmd"
sed -i "s|%GP_HADOOP_TARGET_VERSION%|${GP_HADOOP_TARGET_VERSION}|g" "${CURDIR}/runcmd"
sed -i "s|%HADOOP_PORT%|${HADOOP_PORT}|g" "${CURDIR}/runcmd"

export HADOOP_USER_NAME=hdfs
$HADOOPCMD fs -rm -r /extwrite/* || echo ""
$HADOOPCMD fs -rm -r /mapreduce/* || echo ""
$HADOOPCMD fs -rm -r /mapred/* || echo ""

#${PGREGRESS} --psqldir=$GPHOME/bin/ --init-file=$CURDIR/init_file --schedule=$CURDIR/gphdfs_regress_schedule  --srcdir=$CURDIR/source_replaced --inputdir=$CURDIR/source_replaced --outputdir=$CURDIR/source_replaced
${PGREGRESS} --psqldir=$GPHOME/bin/ --init-file=$CURDIR/init_file --schedule=$CURDIR/limited_schedule --srcdir=$CURDIR/source_replaced --inputdir=$CURDIR/source_replaced --outputdir=$CURDIR/source_replaced
