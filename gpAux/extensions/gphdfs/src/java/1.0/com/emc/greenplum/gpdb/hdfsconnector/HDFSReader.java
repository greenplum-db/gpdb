/*-------------------------------------------------------------------------
 *
 * HDFSReader.java
 *
 * Copyright (c) 2011, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

package com.emc.greenplum.gpdb.hdfsconnector;

import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;

import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FileStatus;

import org.apache.hadoop.mapreduce.*;
import org.apache.hadoop.mapreduce.lib.input.*;
import org.apache.hadoop.io.*;

import java.io.*;
import java.io.DataOutputStream;
import java.util.*;
import java.net.URI;
import java.net.URISyntaxException;


public class HDFSReader
{
    protected Configuration conf;
	protected int           segid;
	protected boolean       isTextFormat;
	
	protected int           totalseg; 
	protected String        inputpath;

    protected ArrayList<InputSplit> segSplits;
    
	/**
	 * Constructor - parse the input args
	 * 
	 * @param args the list of input argument, that should include
	 * local segment id, total number of segments, format (TEXT or GPDBWritable)
	 * and the location in the form of gphdfs://host:port/file_path
	 * 
	 * @throws Exception when something goes wrong
	 */
	public HDFSReader(String[] args) {
		/**
		 * 4 input argments are required:
	     * arg[0] : local segment id
	     * arg[1] : total number of segments
	     * arg[2] : format: TEXT or GPDBWritable
	     * arg[3] : hadoop connector version
	     * arg[4] : gphdfs://host:port/file_path
	     */
	    if (args.length < 5)
	        throw new IllegalArgumentException("Wrong number of argument");

		segid        = Integer.parseInt(args[0]);
		totalseg     = Integer.parseInt(args[1]); 
		isTextFormat = (args[2].equals("TEXT"));
		String connVer = args[3];
	    URI inputURI;
		try {
			inputURI = new URI(args[4]);
		} catch (URISyntaxException e) {
			throw new IllegalArgumentException("Illegal input uri: "+args[4]);
		}

	    // Create a conf, set the HDFS URI and the input path
	    conf = new Configuration();
	    ConnectorUtil.setHadoopFSURI(conf, inputURI, connVer);
	    inputpath = inputURI.getPath();
	}
		
	/** 
	 * Either TEXT or GPDBWritable. Call the appropiate read method
	 * 
	 * @throws Exception when something goes wrong
	 */
	public void doRead() throws IOException, InterruptedException {
		assignSplits();

		if (isTextFormat)
			readTextFormat();
		else
			readGPDBWritableFormat();
	}
	
	/**
	 * Read the input as a TextInputFormat by using LineRecordReader
	 * and write it to Std Out as is
	 *
	 * @throws Exception when something goes wrong
	 */
	protected void readTextFormat() throws IOException {
		TaskAttemptContext fakeTaskCtx = new TaskAttemptContext(conf, new TaskAttemptID());
		
		// For each split, use the record reader to read the stuff
		for(int i=0; i<segSplits.size(); i++) {
			// Text uses LineRecordReader
			LineRecordReader lrr = new LineRecordReader();
			lrr.initialize(segSplits.get(i), fakeTaskCtx);
			while(lrr.nextKeyValue()) {
				Text val = lrr.getCurrentValue();
				byte[] bytes = val.getBytes();
				System.out.write(bytes, 0, val.getLength());
				System.out.println();
				System.out.flush();
			}
			lrr.close();
		}
	}
	
	/**
	 * Read the input as GPDBWritable from SequenceFile
	 * and write it to Std Out
	 * 
	 * @throws Exception when something goes wrong
	 */
	protected void readGPDBWritableFormat() throws IOException, InterruptedException {
		TaskAttemptContext fakeTaskCtx = new TaskAttemptContext(conf, new TaskAttemptID());
		
		/*
		 *  For each split, use the record reader to read the GPDBWritable.
		 *  Then GPDBWritable will write the serialized form to standard out,
		 *  which will be consumed by GPDB's gphdfsformatter.
		 */
		DataOutputStream dos = new DataOutputStream(System.out);
		for(int i=0; i<segSplits.size(); i++) {
			// GPDBWritable uses SequenceFileRecordReader
			SequenceFileRecordReader<LongWritable, GPDBWritable> sfrr =
				new SequenceFileRecordReader<LongWritable, GPDBWritable>();
			sfrr.initialize(segSplits.get(i), fakeTaskCtx);
			while(sfrr.nextKeyValue()) {
				GPDBWritable val = sfrr.getCurrentValue();
				val.write(dos);
				dos.flush();
			}
			sfrr.close();
		}
	}

	/**
	 * Create a list of input splits from the input path
	 * and then assign splits to our segment in round robin
	 */
	protected void assignSplits() throws IOException {
		// Create a input split list for the input path
		Job jobCtx = new Job(conf);
		FileInputFormat.setInputPaths(jobCtx, new Path(inputpath));
		FileInputFormat fformat = (isTextFormat)? new TextInputFormat() : new SequenceFileInputFormat(); 
		List<InputSplit> splits = fformat.getSplits(jobCtx);

		// Round robin assign splits to segments
		segSplits = new ArrayList<InputSplit>(splits.size()/totalseg+1);
		for(int i=0; i<splits.size(); i++) {
			if (i%totalseg == segid)
				segSplits.add(splits.get(i));
		}
	}
	
	public static void main(String[] args) throws Exception
	{
		HDFSReader hr = new HDFSReader(args);
		hr.doRead();
	} 	
}
