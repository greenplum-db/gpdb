package com.emc.greenplum.gpdb.hadoop.formathandler;

import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.parquet.column.ParquetProperties.WriterVersion;
import org.apache.parquet.example.data.Group;
import org.apache.parquet.example.data.simple.SimpleGroupFactory;
import org.apache.parquet.hadoop.ParquetWriter;
import org.apache.parquet.hadoop.example.GroupWriteSupport;
import org.apache.parquet.hadoop.metadata.CompressionCodecName;
import org.apache.parquet.io.api.Binary;
import org.apache.parquet.schema.MessageType;
import org.apache.parquet.schema.MessageTypeParser;
import org.apache.parquet.schema.OriginalType;
import org.apache.parquet.schema.PrimitiveType;
import org.apache.parquet.schema.Type;
import org.apache.parquet.schema.Types;
import org.apache.parquet.schema.PrimitiveType.PrimitiveTypeName;
import org.apache.parquet.schema.Type.Repetition;

import com.emc.greenplum.gpdb.hadoop.formathandler.util.FormatHandlerUtil;
import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;
import com.emc.greenplum.gpdb.hdfsconnector.ColumnSchema;

public class GpdbParquetFileWriter {
	private static final String SNAPPY_COMPRESS = "snappy";
	private static final String LZO_COMPRESS = "lzo";
	private static final String GZIP_COMPRESS = "gz";

	private static final WriterVersion DEFAULT_PARQUET_VERSION = WriterVersion.PARQUET_1_0;

	private static final int DEFAULT_PAGE_SIZE = 1 * 1024 * 1024;
	private static final int DEFAULT_DICTIONARY_PAGE_SIZE = 512 * 1024;
	private static final int DEFAULT_ROWGROUP_SIZE = 8* 1024 *1024;

	Configuration conf = null;
	String outputPath = null;
	boolean isCompressed = false;
	String compressCodec = null;
	String compressType = null;
	String parquetSchemaFile = null;
	String namespace = null;
	int pageSize = DEFAULT_PAGE_SIZE;
	int rowGroupSize = DEFAULT_ROWGROUP_SIZE;
	WriterVersion parquetVersion = DEFAULT_PARQUET_VERSION;
	int dicPageSize = DEFAULT_DICTIONARY_PAGE_SIZE;
	boolean dicEnable = false;

	List<ColumnSchema> columnSchemas = new ArrayList<ColumnSchema>();

	public GpdbParquetFileWriter(Configuration conf, String outputpath,
			boolean isCompressed, String compressCodec, String compressType, String schemaFile,
			String namespace, int pageSize, int rowGroupSize, String parquetVersion, int dicPageSize, boolean dicEnable) {
		this.conf = conf;
		this.outputPath = outputpath;
		this.isCompressed = isCompressed;
		this.compressCodec = compressCodec;
		this.compressType = compressType;
		this.parquetSchemaFile = schemaFile;
		this.namespace = namespace;

		if (pageSize != -1) {
			this.pageSize = pageSize;
		}

		if (rowGroupSize != -1) {
			this.rowGroupSize = rowGroupSize;
		}

		if (parquetVersion != null) {
			this.parquetVersion = WriterVersion.fromString(parquetVersion);
		}

		if (dicPageSize != -1) {
			this.dicPageSize = dicPageSize;
		}

		this.dicEnable = dicEnable;
	}

	public GpdbParquetFileWriter(){}

	/**
	 * read GPDBWritable from gpdb and then write it to hdfs
	 *
	 * @throws Exception when something goes wrong
	 */
	public void doWrite() throws IOException {
//		if there is no schema provided by user, we will read schema later
		ParquetWriter<Group> dataFileWriter = null;
		DataInputStream dis = new DataInputStream(System.in);
		try {
			MessageType schema = null;
			SimpleGroupFactory groupFactory = null;

//			read table structure info and auto-gen avro schema
			schema = autoGenSchema(dis);
//			int total = dis.readInt();//skip the original 4 byte VARHDSZ

			if (parquetSchemaFile != null) {
//				if user give us a schema file, read schema from it
				String schemaString = readSchemaFile(parquetSchemaFile);
				schema = MessageTypeParser.parseMessageType(schemaString);
			}

			GroupWriteSupport.setSchema(schema, conf);

			CompressionCodecName codecName = CompressionCodecName.UNCOMPRESSED;
			if (isCompressed) {
				if (compressCodec.equals(LZO_COMPRESS)) {
					codecName = CompressionCodecName.LZO;
				}else if (compressCodec.equals(SNAPPY_COMPRESS)) {
					codecName = CompressionCodecName.SNAPPY;
				}else if (compressCodec.equals(GZIP_COMPRESS)) {
					codecName = CompressionCodecName.GZIP;
				}else {
					throw new IOException("compression method not support, codec:" + compressCodec);
				}
			}

			dataFileWriter = new ParquetWriter<Group>(
		            new Path(outputPath),
		            new GroupWriteSupport(),
		            codecName, rowGroupSize, pageSize, dicPageSize, dicEnable, false, parquetVersion, conf);

			groupFactory = new SimpleGroupFactory(schema);

			while (true) {
				GPDBWritable gw = new GPDBWritable();
				gw.readFields(dis);

				Group pqGroup = groupFactory.newGroup();

				fillRecord(pqGroup, gw, schema);

				dataFileWriter.write(pqGroup);
			}
		} catch (EOFException e) {
			// this is ok, read the end of input stream, keep error msg for testing
			//e.printStackTrace();
		} finally {
			if (dataFileWriter != null) {
				dataFileWriter.close();
			}

			dis.close();
		}
	}

	private String readSchemaFile(String pqSchemaFile) throws IOException {
		Path parquetSchemaPath = new Path(parquetSchemaFile);
		FileSystem schemaFs = parquetSchemaPath.getFileSystem(conf);
		BufferedReader bufReader = new BufferedReader(new InputStreamReader(schemaFs.open(parquetSchemaPath)));

		String schemaString = "", tmpString = null;
		while ((tmpString = bufReader.readLine()) != null) {
			schemaString += tmpString;
		}
		return schemaString;
	}

	/**
	 * fill group using GPDBWritable
	 *
	 * @throws IOException
	 */
	public void fillRecord(Group pqGroup, GPDBWritable gw, MessageType schema) throws IOException{
		int[] colType = gw.getColumnType();
		List<Type> fields = schema.getFields();

		for (int i = 0; i < colType.length; i++) {
			if (!gw.isNull(i)) {
				fillElement(i, colType[i], pqGroup, gw, fields.get(i));
			}
		}
	}

	private void fillElement(int index, int colType, Group pqGroup, GPDBWritable gw,Type field) throws IOException {
		switch (colType) {
			case GPDBWritable.BPCHAR:
			case GPDBWritable.CHAR:
			case GPDBWritable.DATE:
			case GPDBWritable.NUMERIC:
			case GPDBWritable.TIME:
			case GPDBWritable.TIMESTAMP:
			case GPDBWritable.VARCHAR:
			case GPDBWritable.TEXT:
//				utf8 or array
				if (field.getRepetition() == Repetition.REPEATED) {
					decodeArrayString(index, field, pqGroup, gw.getString(index), columnSchemas.get(index).getDelim());
				}else {
					int gpdbType = columnSchemas.get(index).getType();
					PrimitiveTypeName priType = field.asPrimitiveType().getPrimitiveTypeName();
					OriginalType originalType = field.getOriginalType();

					if (gpdbType == GPDBWritable.NUMERIC && priType == PrimitiveTypeName.INT32) {
						pqGroup.add(index, Integer.parseInt(gw.getString(index)));
					}else if (gpdbType == GPDBWritable.NUMERIC && priType == PrimitiveTypeName.INT64) {
						pqGroup.add(index, Long.parseLong(gw.getString(index)));
					}else if (gpdbType == GPDBWritable.DATE && priType == PrimitiveTypeName.INT32) {
						pqGroup.add(index, (int)FormatHandlerUtil.getTimeDiff(gw.getString(index), "1970-01-01", "yyyy-mm-dd", 24*60*60*1000));
					}else if (gpdbType == GPDBWritable.TIME && priType == PrimitiveTypeName.INT32) {
						pqGroup.add(index, (int)FormatHandlerUtil.getTimeDiff(gw.getString(index), "00:00:00", "mm:hh:ss", 1));
					}else if (gpdbType == GPDBWritable.TIMESTAMP && priType == PrimitiveTypeName.INT64) {
						pqGroup.add(index, FormatHandlerUtil.getTimeDiff(gw.getString(index), "1970-01-01 00:00:00", "yyyy-mm-dd mm:hh:ss", 1));
					}else if (gpdbType == GPDBWritable.INTERVAL && originalType == OriginalType.INTERVAL ) {
//						interval is complex, we just use string, for now, we just support 'postgres' style interval
//						1 year 2 mons -3 days +04:05:06.00901
						byte[] interval = FormatHandlerUtil.getParquetInterval(gw.getString(index));
						pqGroup.add(index, Binary.fromByteArray(interval));
					}else {
						pqGroup.add(index, gw.getString(index));
					}
				}
				break;

			case GPDBWritable.BYTEA:
				pqGroup.add(index, Binary.fromByteArray(gw.getBytes(index)));
				break;

			case GPDBWritable.REAL:
				pqGroup.add(index, gw.getFloat(index));
				break;

			case GPDBWritable.BIGINT:
				pqGroup.add(index, gw.getLong(index));
				break;

			case GPDBWritable.BOOLEAN:
				pqGroup.add(index, gw.getBoolean(index));
				break;

			case GPDBWritable.FLOAT8:
				pqGroup.add(index, gw.getDouble(index));
				break;

			case GPDBWritable.INTEGER:
				pqGroup.add(index, gw.getInt(index));
				break;

			case GPDBWritable.SMALLINT:
				pqGroup.add(index, gw.getShort(index));
				break;

			default:
				throw new IOException("internal error, not support type, typeId:" + colType);
			}
	}

	private void decodeArrayString(int index, Type field, Group pqGroup, String arrayString, char delim) throws IOException {
//		for parquet, we only have one-dimention array
//		anotation support: decimal, time, timestamp
		String[] splits = FormatHandlerUtil.getArraySplits(arrayString.toCharArray(), delim);

		for (String elementString : splits) {
			switch (field.asPrimitiveType().getPrimitiveTypeName()) {
			case BOOLEAN:
				pqGroup.add(index, Boolean.parseBoolean(elementString));
				break;

			case INT32:
				if (columnSchemas.get(index).getType() == GPDBWritable.DATE) {
					pqGroup.add(index, (int)FormatHandlerUtil.getTimeDiff(elementString, "1970-01-01", "yyyy-mm-dd", 24*60*60*1000));
				}else if (columnSchemas.get(index).getType() == GPDBWritable.TIME) {
					pqGroup.add(index, (int)FormatHandlerUtil.getTimeDiff(elementString, "00:00:00", "mm:hh:ss", 1));
				}else {
					pqGroup.add(index, Integer.parseInt(elementString));
				}
				break;

			case INT64:
				if (columnSchemas.get(index).getType() == GPDBWritable.TIMESTAMP) {
					pqGroup.add(index, FormatHandlerUtil.getTimeDiff(elementString, "1970-01-01 00:00:00", "yyyy-mm-dd mm:hh:ss", 1));
				}else {
					pqGroup.add(index, Long.parseLong(elementString));
				}
				break;

			case FLOAT:
				pqGroup.add(index, Float.parseFloat(elementString));
				break;

			case DOUBLE:
				pqGroup.add(index, Double.parseDouble(elementString));
				break;

			case INT96:
			case BINARY:
			case FIXED_LEN_BYTE_ARRAY:
				OriginalType type = field.getOriginalType();
				if (type == OriginalType.UTF8 || type == OriginalType.JSON) {
					pqGroup.add(index, elementString);
				}else if (type == OriginalType.DECIMAL) {
					pqGroup.add(index, Binary.fromByteArray(elementString.getBytes()));
				}else if (type == OriginalType.INTERVAL) {
					pqGroup.add(index, Binary.fromByteArray(FormatHandlerUtil.getParquetInterval(elementString)));
				}else {
					pqGroup.add(index, Binary.fromByteArray(FormatHandlerUtil.octString2byteArray(elementString).array()));
				}
				break;

			default:
				throw new IOException("internal error, you should not be here, pqtype:" + field.asPrimitiveType().getPrimitiveTypeName());
			}
		}
	}

	/**
	 * generate schema automatically
	 *
	 * @throws IOException
	 */
	private MessageType autoGenSchema(DataInputStream dis) throws IOException {
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
		List<Type> fields = new ArrayList<Type>();
		for (int i = 0; i < colCount; i++) {
			int colLen = dis.readInt();
			byte[] colName = new byte[colLen];
			dis.readFully(colName);

			int colType = dis.readInt();
			int notNull = dis.readByte();
			int ndims = dis.readInt();
			char delim = (char)dis.readByte();

			columnSchemas.add(new ColumnSchema(new String(colName), colType, notNull, ndims, delim));
			fields.add(generateParquetType(new String(colName), colType, notNull, ndims));
		}

		return new MessageType(tableName, fields);
	}

	private Type generateParquetType(String colName, int colType, int notNull, int ndims) {
		PqElementType elementType = generateElementType(colType, notNull, ndims);

		if (elementType.originalType != null) {
			return new PrimitiveType(elementType.isArray?Repetition.REPEATED:Repetition.OPTIONAL, 
					elementType.primitiveType, colName, elementType.originalType);
		}else {
			return new PrimitiveType(elementType.isArray?Repetition.REPEATED:Repetition.OPTIONAL, 
					elementType.primitiveType, colName);
		}
	}

	private PqElementType generateElementType(int colType, int notNull, int ndims) {
		PqElementType eType = new PqElementType();

		if (GPDBWritable.isArrayType(colType, ndims)) {
			eType.isArray = true;
			colType = GPDBWritable.getElementTypeFromArrayType(colType);
		}

		switch (colType) {
		case GPDBWritable.BOOLEAN:
			eType.primitiveType = PrimitiveTypeName.BOOLEAN;
			break;

		case GPDBWritable.BYTEA:
			eType.primitiveType = PrimitiveTypeName.BINARY;
			break;

		case GPDBWritable.BIGINT:
			eType.primitiveType = PrimitiveTypeName.INT64;
			break;

		case GPDBWritable.SMALLINT:
			eType.originalType = OriginalType.INT_16;
		case GPDBWritable.INTEGER:
			eType.primitiveType = PrimitiveTypeName.INT32;
			break;

		case GPDBWritable.REAL:
			eType.primitiveType = PrimitiveTypeName.FLOAT;
			break;

		case GPDBWritable.FLOAT8:
			eType.primitiveType = PrimitiveTypeName.DOUBLE;
			break;

		case GPDBWritable.CHAR:
		case GPDBWritable.VARCHAR:
		case GPDBWritable.BPCHAR:
		case GPDBWritable.NUMERIC:
		case GPDBWritable.DATE:
		case GPDBWritable.TIME:
		case GPDBWritable.TIMESTAMP:
		case GPDBWritable.TEXT:
		default:
//			others we just treat them as text, may be udt
			eType.primitiveType = PrimitiveTypeName.BINARY;

			if (colType == GPDBWritable.NUMERIC) {
				eType.originalType = OriginalType.DECIMAL;
			}else {
				eType.originalType = OriginalType.UTF8;
			}
		}

		return eType;
	}

	class PqElementType{
		protected boolean isArray = false;
		protected PrimitiveTypeName primitiveType = null;
		protected OriginalType originalType = null;
	}
}
