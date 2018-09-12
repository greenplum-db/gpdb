/*-------------------------------------------------------------------------
 *
 * HDFSReader.java
 *
 * Copyright (c) 2011, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

package com.emc.greenplum.gpdb.hdfsconnector;

import com.emc.greenplum.gpdb.hadoop.formathandler.AvroFileReader;
import com.emc.greenplum.gpdb.hadoop.formathandler.GpdbParquetFileReader;
import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.mapreduce.*;
import org.apache.hadoop.mapreduce.lib.input.*;
import org.apache.hadoop.mapreduce.task.TaskAttemptContextImpl;
import org.apache.hadoop.io.*;

import java.io.*;
import java.util.*;
import java.net.URI;
import java.net.URISyntaxException;


public class HDFSReader
{
	protected Configuration conf;
	protected int           segid;
	protected boolean       isTextFormat;
	protected boolean		isAvroFormat;
	protected boolean		isParquetFormat;
	protected int           totalseg; 
	protected String        inputpath;

	protected String		schemaFile = null;

	protected boolean		schemaComplete = false;

	protected boolean		autoSelect = false;

	protected ArrayList<InputSplit> segSplits;

	protected List<ColumnSchema> tableSchema = null;

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
	     * arg[2] : format: TEXT or GPDBWritable, AVRO, PARQUET
	     * arg[3] : hadoop connector version
	     * arg[4] : gphdfs://host:port/file_path
	     * arg[5] : table schema
	     * arg[6] : table column name list
	     */
	    if (args.length < 5)
	        throw new IllegalArgumentException("Wrong number of argument");

		segid        = Integer.parseInt(args[0]);
		totalseg     = Integer.parseInt(args[1]); 
		isTextFormat = (args[2].equals("TEXT"));
		isAvroFormat = (args[2].equals("AVRO"));
		isParquetFormat = (args[2].equals("PARQUET"));

		String connVer = args[3];
	    URI inputURI;
		try {
			inputURI = new URI(args[4]);
		} catch (URISyntaxException e) {
			throw new IllegalArgumentException("Illegal input uri: "+args[4]);
		}

		if (args.length >= 6) {
//			extract table schema
//			format : sprintf(args[5], "%010d%d%d%03d", typid, rel->rd_att->attrs[i]->attnotnull, rel->rd_att->attrs[i]->attndims, delim);
			int i = 0;
			while (i < args[5].length()) {
				int typeId = Integer.parseInt(args[5].substring(i, i + 10));
				i += 10;
				int notNull = args[5].charAt(i) - '0';
				i++;
				int ndims = args[5].charAt(i) - '0';
				i++;
				char delim = (char)Integer.parseInt(args[5].substring(i, i + 3));
				i += 3;

				if (tableSchema == null) {
					tableSchema = new ArrayList<ColumnSchema>();
				}

				tableSchema.add(new ColumnSchema(typeId, notNull, ndims, delim));
			}

			if (args.length >= 7) {
//				extract table column names
//				format : col1,col2,...,SCHEMA_NAME_MAGIC
				String[] colNames = args[6].split(",");
				if (colNames.length == tableSchema.size() && args[6].charAt(args[6].length() - 1) == ',') {
//					we got the whole column name list
					for (int j = 0; j < colNames.length; j++) {
						tableSchema.get(j).setColumName(colNames[j]);
					}

					schemaComplete = true;
				}
			}
		}

		if (inputURI.getQuery() != null) {
			String[] kv_pair = inputURI.getQuery().split("&");
			for(int i=0; i<kv_pair.length; i++) {
				String[] kv = kv_pair[i].split("=");
				String key = kv[0].toLowerCase();
				String val = kv[1];

				if (key.equals("schema")) {
					schemaFile = val;
				}else if (key.equals("autoselect")) {
					autoSelect = ConnectorUtil.getBoolean(key, val);
				}
			}
		}

        // Create a conf, set the HDFS URI and the input path
        //  Configuration.addDefaultResource("hdfs-site.xml");//hdfs-site.xml
        //  here will be overlapped by hdfs-default.xml
        conf = new Configuration();
        conf.addResource("hdfs-site.xml");//this line is not needed now, but line 73 
        //  may related with some unknow old hadoop version, so I changed it here

	    ConnectorUtil.setHadoopFSURI(conf, inputURI, connVer);
	    inputpath = inputURI.getPath();
	}

	/** 
	 * Either TEXT or GPDBWritable. Call the appropiate read method
	 * 
	 * @throws Exception when something goes wrong
	 */
	public void doRead() throws IOException, InterruptedException {
		ConnectorUtil.loginSecureHadoop(conf);

		if (isTextFormat) {
			assignSplits();

			readTextFormat();
		}
		else if (isAvroFormat) {
			AvroFileReader avroReader = new AvroFileReader(conf, segid, totalseg, inputpath, tableSchema, schemaFile, schemaComplete, autoSelect, System.out);
			avroReader.readAvroFormat();
		}
		else if (isParquetFormat) {
			GpdbParquetFileReader parquetReader = new GpdbParquetFileReader(conf, segid, totalseg, inputpath, tableSchema, schemaComplete, autoSelect, System.out);
			parquetReader.readParquetFormat();
		}
		else {
			assignSplits();

			readGPDBWritableFormat();
		}
	}

	/**
	 * Read the input as a TextInputFormat by using LineRecordReader
	 * and write it to Std Out as is
	 *
	 * @throws Exception when something goes wrong
	 */
	protected void readTextFormat() throws IOException {
		TaskAttemptContext fakeTaskCtx =
			new TaskAttemptContextImpl(conf, new TaskAttemptID());

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
		TaskAttemptContext fakeTaskCtx =
			new TaskAttemptContextImpl(conf, new TaskAttemptID());

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
