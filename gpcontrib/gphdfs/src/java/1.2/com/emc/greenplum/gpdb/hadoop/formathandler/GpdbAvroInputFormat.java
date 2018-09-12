package com.emc.greenplum.gpdb.hadoop.formathandler;

import java.io.IOException;

import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.mapred.FileInputFormat;
import org.apache.hadoop.mapred.InputSplit;
import org.apache.hadoop.mapred.JobConf;
import org.apache.hadoop.mapred.RecordReader;
import org.apache.hadoop.mapred.Reporter;

public class GpdbAvroInputFormat extends FileInputFormat{
	@Override
	public RecordReader getRecordReader(InputSplit split,JobConf conf,Reporter reporter) throws IOException {
		throw new UnsupportedOperationException("GpdbInputFormat should not be used for reading data, but only for obtaining the splits of a file");
	}

	/*
	 * Return true if this file can be split.
	 */
	@Override
	protected boolean isSplitable(FileSystem fs, Path filename) {
		return true;
	}
}
