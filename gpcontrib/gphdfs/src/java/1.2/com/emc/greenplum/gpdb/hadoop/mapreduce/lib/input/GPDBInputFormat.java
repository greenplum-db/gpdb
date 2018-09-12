package com.emc.greenplum.gpdb.hadoop.mapreduce.lib.input;

import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;
import org.apache.hadoop.io.SequenceFile;
import org.apache.hadoop.io.LongWritable;
import org.apache.hadoop.mapreduce.lib.input.SequenceFileInputFormat;

/**
 * An {@link org.apache.hadoop.mapreduce.InputFormat} that read GPDB binary format from HDFS.
 * @author achoi
 *
 */
public class GPDBInputFormat<K extends LongWritable, V extends GPDBWritable>
	extends SequenceFileInputFormat<K, V> {
	
}