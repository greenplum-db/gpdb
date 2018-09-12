package com.emc.greenplum.gpdb.hadoop.mapreduce.lib.output;

import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;
import org.apache.hadoop.io.SequenceFile;
import org.apache.hadoop.io.LongWritable;
import org.apache.hadoop.mapreduce.lib.output.SequenceFileOutputFormat;

/**
 * An {@link org.apache.hadoop.mapreduce.OutputFormat} that writes GPDB binary format on HDFS.
 * @author achoi
 *
 */
public class GPDBOutputFormat<K extends LongWritable, V extends GPDBWritable>
	extends SequenceFileOutputFormat<K, V> {
	
}