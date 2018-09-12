package com.emc.greenplum.gpdb.hadoop.mapred;

import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;
import org.apache.hadoop.io.SequenceFile;
import org.apache.hadoop.io.LongWritable;
import org.apache.hadoop.mapred.SequenceFileInputFormat;

/**
 * An {@link org.apache.hadoop.mapred.InputFormat} that read GPDB binary format from HDFS.
 * @author achoi
 *
 */
public class GPDBInputFormat<K extends LongWritable, V extends GPDBWritable>
	extends SequenceFileInputFormat<K, V> {
	
}