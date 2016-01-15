package com.emc.greenplum.gpdb.hadoop.formathandler;

import java.io.DataInputStream;
import java.io.EOFException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

import org.apache.avro.Schema;
import org.apache.avro.Schema.Field;
import org.apache.avro.Schema.Type;
import org.apache.avro.file.CodecFactory;
import org.apache.avro.file.DataFileWriter;
import org.apache.avro.generic.GenericData;
import org.apache.avro.generic.GenericDatumWriter;
import org.apache.avro.generic.GenericRecord;
import org.apache.avro.io.DatumWriter;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;

import com.emc.greenplum.gpdb.hadoop.formathandler.util.FormatHandlerUtil;
import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;

public class AvroFileWriter {
	private static final int DEFAULT_DEFLATE_LEVEL = 6;
	private static final String SNAPPY_COMPRESS = "snappy";
	private static final String COMMON_NAMESPACE = "public.avro";
	public static String BLOCK_COMPRESSION = "block"; // for compression type

	Configuration conf = null;
	String outputPath = null;
	boolean isCompressed = false;
	String compressCodec = null;
	String compressType = null;
	String avroSchemaFile = null;
	String namespace = null;
	int codecLevel = DEFAULT_DEFLATE_LEVEL;

	char[] columDelimiter = null;

	public AvroFileWriter(Configuration conf, String outputpath,
			boolean isCompressed, String compressCodec, String compressType, String schemaFile, int codecLevel, String namespace) {
		this.conf = conf;
		this.outputPath = outputpath;
		this.isCompressed = isCompressed;
		this.compressCodec = compressCodec;
		this.compressType = compressType;
		this.avroSchemaFile = schemaFile;
		this.namespace = namespace;

		if (codecLevel != -1) {
			this.codecLevel = codecLevel;
		}
	}

	public AvroFileWriter(){}

	/**
	 * read GPDBWritable from gpdb and then write it to hdfs
	 *
	 * @throws Exception when something goes wrong
	 */
	public void doWrite() throws IOException {
//		if there is no schema provided by user, we will read schema later

		DataFileWriter<GenericRecord> dataFileWriter = null;
		DataInputStream dis = new DataInputStream(System.in);
		try {
//			read table structure info and auto-gen avro schema
			Schema schema = autoGenSchema(dis);
//			int total = dis.readInt();//skip the original 4 byte VARHDSZ

			if (avroSchemaFile != null) {
//				if user give us a schema file, read schema from it
				Path avroSchemaPath = new Path(avroSchemaFile);
				FileSystem schemaFs = avroSchemaPath.getFileSystem(conf);
				schema = new Schema.Parser().parse(schemaFs.open(avroSchemaPath));
			}

			DatumWriter<GenericRecord> datumWriter = new GenericDatumWriter<GenericRecord>(schema);
			dataFileWriter = new DataFileWriter<GenericRecord>(datumWriter);
			if (isCompressed) {
				if (compressCodec != null && compressCodec.equals(SNAPPY_COMPRESS)) {
					dataFileWriter.setCodec(CodecFactory.snappyCodec());
				}else {
//						use deflate codec as our default codec for avro
					dataFileWriter.setCodec(CodecFactory.deflateCodec(codecLevel));//avro-1.7.7.jar:TestDataFileDeflate.java
				}
			}

			Path file = new Path(outputPath);
			FileSystem fs = file.getFileSystem(conf);
			FSDataOutputStream avroOut = fs.create(file, false);
			dataFileWriter.create(schema, avroOut);

			while (true) {
				GPDBWritable gw = new GPDBWritable();
				gw.readFields(dis);

				GenericRecord record = new GenericData.Record(schema);
				fillRecord(record, gw, schema);

				dataFileWriter.append(record);
			}
		} catch (EOFException e) {
			// this is ok, read the end of input stream, keep error msg for testing
			//e.printStackTrace();
		}

		dataFileWriter.close();
	}

	/**
	 * generate schema automatically
	 *
	 * @throws IOException
	 */
	private Schema autoGenSchema(DataInputStream dis) throws IOException {
		dis.readInt();//skip schema-head length

		short version = dis.readShort();
		if (version !=  GPDBWritable.SCHEMA_VERSION) {
			throw new IOException("schema version mismatched, should: 2 now:" + version);
		}

		int tableNameLen = dis.readInt();
		byte[] tableNameBytes = new byte[tableNameLen];
		dis.readFully(tableNameBytes);
		String tableName = new String(tableNameBytes);

		int colCount = dis.readInt();
		String[] colNames = new String[colCount];
		int[] colTypes = new int[colCount];
		int[] notNull = new int[colCount];
		int[] arrNDims = new int[colCount];
		columDelimiter = new char[colCount];

		for (int i = 0; i < colCount; i++) {
			int colLen = dis.readInt();
			byte[] colName = new byte[colLen];
			dis.readFully(colName);
			colNames[i] = new String(colName);

			colTypes[i] = dis.readInt();

			notNull[i] = dis.readByte();

			arrNDims[i] = dis.readInt();

			columDelimiter[i] = (char)dis.readByte();
		}

		String ns = (namespace == null) ?COMMON_NAMESPACE:namespace;
		Schema schema = Schema.createRecord(tableName, "", ns, false);

		List<Field> fields = new ArrayList<Field>();
		for (int i = 0; i < colCount; i++) {
			fields.add(new Field(colNames[i], getFiledSchema(colTypes[i], notNull[i], arrNDims[i]), "", null));
		}

		schema.setFields(fields);

		return schema;
	}

	/**
	 * get filed schema according to the given type
	 *
	 * @throws IOException
	 */
	private Schema getFiledSchema(int type, int notNull, int dim) throws IOException {
		List<Schema> unionList = new ArrayList<Schema>();
//		in this version of gpdb, external table should not set 'notnull' attribute
		unionList.add(Schema.create(Type.NULL));

		switch (type) {
		case GPDBWritable.BOOLEAN:
			unionList.add(Schema.create(Type.BOOLEAN));
			break;
		case GPDBWritable.BYTEA:
			unionList.add(Schema.create(Type.BYTES));
			break;
		case GPDBWritable.BIGINT:
			unionList.add(Schema.create(Type.LONG));
			break;
		case GPDBWritable.SMALLINT:
		case GPDBWritable.INTEGER:
			unionList.add(Schema.create(Type.INT));
			break;
		case GPDBWritable.REAL:
			unionList.add(Schema.create(Type.FLOAT));
			break;
		case GPDBWritable.FLOAT8:
			unionList.add(Schema.create(Type.DOUBLE));
			break;
		case GPDBWritable.CHAR:
		case GPDBWritable.VARCHAR:
		case GPDBWritable.BPCHAR:
		case GPDBWritable.NUMERIC:
		case GPDBWritable.DATE:
		case GPDBWritable.TIME:
		case GPDBWritable.TIMESTAMP:
		case GPDBWritable.TEXT:
			unionList.add(Schema.create(Type.STRING));
			break;
		default:
//			may be array or some type else
			if (isArrayType(type, dim)) {
				int elementType = getElementType(type);
				Schema array = Schema.createArray(getFiledSchema(elementType, notNull, 0));
				for (int i = 1; i < dim; i++) {
//					for multi-dim array
					array = Schema.createArray(array);
				}

				unionList.add(array);
			}else {
//				others we just treate them as text
				unionList.add(Schema.create(Type.STRING));
			}
		}

		return Schema.createUnion(unionList);
	}

	/**
	 * get array's element type
	 */
	private int getElementType(int type) {
		return GPDBWritable.getElementTypeFromArrayType(type);
	}

	/**
	 * check whether the type is an array
	 */
	private boolean isArrayType(int type, int dim) {
		return dim > 0;
//		return GPDBWritable.isArrayType(type);
	}

	/**
	 * decode GPDBWritable according to the schema, and generate a record
	 *
	 * @throws Exception when something goes wrong
	 */
	public void fillRecord(GenericRecord record, GPDBWritable gw, Schema schema) throws IOException {
		int[] colType = gw.getColumnType();
		Object[] colValue = gw.getColumnValue();

		for (int i = 0; i < colType.length; i++) {
			switch (colType[i]) {
			case GPDBWritable.BPCHAR:
			case GPDBWritable.CHAR:
			case GPDBWritable.DATE:
			case GPDBWritable.NUMERIC:
			case GPDBWritable.TIME:
			case GPDBWritable.TIMESTAMP:
			case GPDBWritable.VARCHAR:
			case GPDBWritable.TEXT:
//				utf8 or array
				record.put(i, FormatHandlerUtil.decodeString(schema.getFields().get(i).schema(), (String)colValue[i], true, columDelimiter[i]));
				break;
			case GPDBWritable.BYTEA:
				record.put(i, colValue[i] == null ? null : ByteBuffer.wrap((byte[])colValue[i]));
				break;
			case GPDBWritable.REAL:
				record.put(i, (Float)colValue[i]);
				break;
			case GPDBWritable.BIGINT:
				record.put(i, (Long)colValue[i]);
				break;
			case GPDBWritable.BOOLEAN:
				record.put(i, (Boolean)colValue[i]);
				break;
			case GPDBWritable.FLOAT8:
				record.put(i, (Double)colValue[i]);
				break;
			case GPDBWritable.INTEGER:
				record.put(i, (Integer)colValue[i]);
				break;
			case GPDBWritable.SMALLINT:
				record.put(i, colValue[i] == null ? null : ((Short)colValue[i]).intValue());
				break;
			default:
				break;
			}
		}
	}
}
