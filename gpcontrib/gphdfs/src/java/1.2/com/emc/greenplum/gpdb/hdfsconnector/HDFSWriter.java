/*-------------------------------------------------------------------------
 *
 * HDFSWritaber.java
 *
 * Copyright (c) 2011, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */


package com.emc.greenplum.gpdb.hdfsconnector;

import com.emc.greenplum.gpdb.hadoop.formathandler.AvroFileWriter;
import com.emc.greenplum.gpdb.hadoop.formathandler.GpdbParquetFileWriter;
import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;

import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.io.*;
import org.apache.hadoop.util.*;
import org.apache.hadoop.mapreduce.*;
import org.apache.hadoop.mapreduce.task.TaskAttemptContextImpl;
import org.apache.hadoop.io.compress.*;
import org.apache.hadoop.mapreduce.lib.output.*;

import java.io.*;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.ByteBuffer;

public class HDFSWriter
{
    protected Configuration conf;
	protected int           segid;
	protected boolean       isTextFormat;
	protected boolean		isAvroFormat;
	protected boolean		isParquetFormat;

	protected String        xid;
	protected String        outputpath;
	protected TaskAttemptContext fakeTaskCtx;

	protected String	schemaPath = null;

	protected boolean isCompressed = false;
	protected String  compressType = null;
	protected String  compressCodec = null;
	protected int codecLevel = -1;
	protected String namespace = null;

//	for parquet only
	protected int pageSize = -1;
	protected int rowgroupSize = -1;
	protected String parquetVersion = null;
	protected int dicPageSize = -1;
	protected boolean dicEnable = false;

	private InputStream instream;
    
	/**
	 * Constructor - parse the input args
	 * 
 	 * @param args the list of input argument, that must be
	 * local segment id, xid, format (TEXT or GPDBWritable)
	 * and the location in the form of gphdfs://host:port/file_path
	 * @throws IOException 
	 * 
	 * @throws Exception when something goes wrong

	 */
	public HDFSWriter(String[] args) {
	    /** 
	     * The following input argments are required:
	     * arg[0] : local segment id
         * arg[1] : xid
	     * arg[2] : format: TEXT / GPDBWritable / Avro
	     * arg[3] : hadoop connector version
	     * arg[4] : gphdfs://host:port/file_path?<query>
	     * query contains:
	     *  compress: true or false
	     * 	compression_type (for gpdbwritable only): BLOCK and RECORD (gpdbwritable)
	     *  codec (for gpdbwritable only): codec class name
	     */
	    if (args.length != 5)
	        throw new IllegalArgumentException("Wrong number of argument");

		segid         = Integer.parseInt(args[0]);
		xid           = args[1]; 
		isTextFormat  = (args[2].equals("TEXT"));
		isAvroFormat  = args[2].equals("AVRO");
		isParquetFormat = args[2].equals("PARQUET");

		String connVer = args[3];
	    URI outputURI;
		try {
			outputURI = new URI(args[4]);
		} catch (URISyntaxException e) {
			throw new IllegalArgumentException("Illegal input uri: "+args[4]);
		}

		/**
		 * Initialize conf, a fake ctx and compression var
		 * Using a fake context is ok because only the confg portion
		 * is used.
		 */
		//Configuration.addDefaultResource("hdfs-site.xml");//hdfs-site.xml here
		//will be overlapped by hdfs-default.xml
        conf        = new Configuration();
        conf.addResource("hdfs-site.xml");//this line is not needed now, but for line 90, 
        //  it may related with some unknow old hadoop version, so I changed it here
		fakeTaskCtx = new TaskAttemptContextImpl(conf, new TaskAttemptID());
		isCompressed = FileOutputFormat.getCompressOutput(fakeTaskCtx);
		compressType = conf.get("mapred.output.compression.type",
				SequenceFile.CompressionType.RECORD.toString());
		compressCodec = conf.get("mapred.output.compression.codec");

		// Set the HDFS URI
		ConnectorUtil.setHadoopFSURI(conf, outputURI, connVer);

		// Setup the output path to <path>/seg_id_xid
		outputpath = outputURI.getPath()+Path.SEPARATOR+segid+"_"+xid;

		// Parse the compression type and codec from the URI query
		if (outputURI.getQuery() != null) {
			String[] kv_pair = outputURI.getQuery().split("&");
			for(int i=0; i<kv_pair.length; i++) {
				String[] kv = kv_pair[i].split("=");
				String key = kv[0].toLowerCase();
				String val = kv[1];

				if (key.equals("compress")) {
					isCompressed  = val.toLowerCase().equals("true");
				}else if (key.equals("compression_type") || key.equals("comptype")) {
					compressType  = val;
				}else if (key.equals("codec")) {
					compressCodec = val;
				}else if (key.equals("codec_level")) {
//					for now, this key is just used with deflate compression in Avro
					codecLevel = ConnectorUtil.getIntBetween(key, val, 1, 9);
				}else if (key.equals("schema")) {
					schemaPath = val;
				}else if (key.equals("namespace")) {
					namespace = val;
				}else if (key.equals("pagesize")) {
					pageSize = ConnectorUtil.getInt(key , val);
				}else if (key.equals("rowgroupsize")) {
					rowgroupSize = ConnectorUtil.getInt(key, val);
				}else if (key.equals("parquetversion") || key.equals("pqversion")) {
					parquetVersion = ConnectorUtil.getStringIn(key, val, "v1", "v2");
				}else if (key.equals("dictionarypagesize") || key.equals("dicpagesize")) {
					dicPageSize = ConnectorUtil.getInt(key, val);
				}else if (key.equals("dictionaryenable") || key.equals("dicenable")) {
					dicEnable = val.toLowerCase().equals("true");
				}else {
					throw new IllegalArgumentException("Unexpected parameter:"+key);
				}
			}
		}

		instream = System.in;
	}


	/** 
	 * Either TEXT or GPDBWritable. Call the appropriate write method
	 */
	public void doWrite() throws IOException, InterruptedException {
		ConnectorUtil.loginSecureHadoop(conf);

		if (isTextFormat)
			writeTextFormat();
		else if (isAvroFormat) {
			AvroFileWriter avroWriter = new AvroFileWriter(conf, outputpath, isCompressed, 
					compressCodec, compressType, schemaPath, codecLevel, namespace);
			avroWriter.doWrite();
		}
		else if (isParquetFormat) {
			GpdbParquetFileWriter parquetWriter = new GpdbParquetFileWriter(conf, outputpath, 
					isCompressed, compressCodec, compressType, schemaPath, namespace, pageSize, 
					rowgroupSize, parquetVersion, dicPageSize, dicEnable);
			parquetWriter.doWrite();
		}
		else
			writeGPDBWritableFormat();
	}

	/**
	 * Helper routine to get the compression codec class
	 */
	protected Class<? extends CompressionCodec> getCodecClass(
			String name, Class<? extends CompressionCodec> defaultValue) {
		Class<? extends CompressionCodec> codecClass = defaultValue;
	    if (name != null) {
	    	try {
	    		codecClass = conf.getClassByName(name).asSubclass(CompressionCodec.class);
	    	} catch (ClassNotFoundException e) {
	    		throw new IllegalArgumentException("Compression codec " + name + " was not found.", e);
	    	}
	    }
	    return codecClass;
	}

	static final int pushBackBufferLen = 15000;

	static int preLen=0;
	static int preBufferLen = 0;
	static byte[] preData = new byte[pushBackBufferLen];

	static int thisLen=0;
	static int thisBufferLen = 0;
	static byte[] thisData = new byte[pushBackBufferLen];

	/**
	 * get line info when error occurs
	 */
	public static String getLineData(){
		return " thisLen: " + thisLen + " thisData:" + bytesToHex(thisData, thisBufferLen)
				+" preLen:" + preLen + " preData:" + bytesToHex(preData, preBufferLen);
	}

	/**
	 * transfer a byte array to a hex string
	 */
	final protected static char[] hexArray = "0123456789ABCDEF".toCharArray();
	private static String bytesToHex(byte[] bytes, int length) {
		if (bytes == null) {
			return "null";
		}

		if (bytes.length == 0 || length == 0) {
			return "empty";
		}

		char[] hexChars = new char[length * 2];
		for ( int j = 0; j < length; j++ ) {
			int v = bytes[j] & 0xFF;
			hexChars[j * 2] = hexArray[v >>> 4];
			hexChars[j * 2 + 1] = hexArray[v & 0x0F];
		}
		return new String(hexChars);
	}

	/**
	 * Write as GPDBOutputFormat, which is SequenceFile <LongWritable, GPDBWritable>
	 */
	protected void writeGPDBWritableFormat() throws IOException, InterruptedException {
		RecordWriter<LongWritable, GPDBWritable> rw = getGPDBRecordWriter();
		LongWritable segIDWritable = new LongWritable(segid);

		PushbackInputStream pStream = new PushbackInputStream(instream, pushBackBufferLen);
		DataInputStream dis = new DataInputStream(pStream);

		// Read from Std-in to construct "value"
		try {
			boolean newVersion = checkNewVersion(pStream);
			if (newVersion) {
				GPDBWritable.skipHead(dis);
			}

			byte[] lenData = new byte[4];

			while (true) {
				int read = pStream.read(lenData);

				ByteBuffer wrapped = ByteBuffer.wrap(lenData);
				thisLen = wrapped.getInt();

				if (read == -1) {
					break;
				} else if (read < 4) {
					thisBufferLen = 0;
					throw new IOException("not enough length data, readLen:" + read + HDFSWriter.getLineData());
				} else {
					thisBufferLen = thisLen - 4;
					if (thisLen > pushBackBufferLen) {
						thisBufferLen = pushBackBufferLen - 4;
					}

					int dataLen = pStream.read(thisData, 0, thisBufferLen);
					if (dataLen < thisBufferLen) {
						thisBufferLen = (dataLen == -1) ? 0 : dataLen;
						throw new IOException("not enough context data, realLen:" + dataLen + HDFSWriter.getLineData());
					}

					pStream.unread(thisData, 0, thisBufferLen);

					pStream.unread(lenData);
				}

				GPDBWritable gw = new GPDBWritable();
				gw.readFields(dis);
				rw.write(segIDWritable, gw);

				preLen = thisLen;
				preBufferLen = thisBufferLen;
				System.arraycopy(thisData, 0, preData, 0, thisBufferLen);
			}
		} catch (EOFException e) {}

		rw.close(fakeTaskCtx);
	}

	/**
	 * check the transporter version
	 */
	private boolean checkNewVersion(PushbackInputStream pStream) throws IOException {
		boolean isNew = false;

		byte[] headVersion = new byte[6];
		pStream.read(headVersion);

		if (headVersion[4] == 0 && headVersion[5] == GPDBWritable.SCHEMA_VERSION) {
//			this is the new version
			isNew = true;
		}

		pStream.unread(headVersion);

		return isNew;
	}


	/**
	 * Write as TextOutputFormat
	 */
	protected void writeTextFormat() throws IOException {
		// construct the output stream
		DataOutputStream dos;
	    FileSystem fs = FileSystem.get(conf);

		// Open the output file based on compression type and codec
		if (!isCompressed) {
			Path file = new Path(outputpath);
			dos = fs.create(file, false);
		}
		else {
			CompressionCodec codec = 
				(CompressionCodec) ReflectionUtils.newInstance(
						getCodecClass(compressCodec, GzipCodec.class),
						conf);
			String extension = codec.getDefaultExtension();

			Path file = new Path(outputpath+extension);
			FSDataOutputStream fileOut = fs.create(file, false);
			dos = new DataOutputStream(codec.createOutputStream(fileOut));
		}

		// read bytes from std-in and write it to the outputstream
		byte[] buf = new byte[1024];
		int  read;
		while((read=instream.read(buf)) > 0)
			dos.write(buf,0,read);

		dos.close();
	}

	/**
	 * Helper function to construct the SequenceFile writer
	 */
	private SequenceFile.Writer getSequenceFileWriter() throws IOException {
		// Get the compression type and codec first
		CompressionCodec codec = null;
		SequenceFile.CompressionType compressionType = SequenceFile.CompressionType.NONE;
		if (isCompressed) {
			compressionType = SequenceFile.CompressionType.valueOf(compressType);
			codec = (CompressionCodec) ReflectionUtils.newInstance(
					getCodecClass(compressCodec, DefaultCodec.class),
					conf);
		}

		// get the path of the  output file 
		Path file = new Path(outputpath);
		FileSystem fs = file.getFileSystem(conf);
		return SequenceFile.createWriter(fs, conf, file,
				LongWritable.class,
				GPDBWritable.class,
				compressionType,
				codec,
				fakeTaskCtx);
	}

	/**
	 * Helper function to construct the SequenceFile writer
	 */
	private RecordWriter<LongWritable, GPDBWritable> getGPDBRecordWriter() throws IOException {
		final SequenceFile.Writer out = getSequenceFileWriter();

		return new RecordWriter<LongWritable, GPDBWritable>() {
			public void write(LongWritable key, GPDBWritable value)
			throws IOException {
				out.append(key, value);
			}

			public void close(TaskAttemptContext context) throws IOException { 
				out.close();
			}
		};
	}


	public static void main(String[] args) throws Exception
	{
		HDFSWriter hw = new HDFSWriter(args);
		hw.doWrite();
	}

}
