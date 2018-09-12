package com.emc.greenplum.gpdb.hadoop.formathandler;

import java.io.DataOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.StringWriter;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import org.apache.avro.Schema;
import org.apache.avro.Schema.Field;
import org.apache.avro.Schema.Type;
import org.apache.avro.file.DataFileReader;
import org.apache.avro.generic.GenericData.Fixed;
import org.apache.avro.generic.GenericDatumReader;
import org.apache.avro.generic.GenericRecord;
import org.apache.avro.io.DatumReader;
import org.apache.avro.mapred.AvroRecordReader;
import org.apache.avro.mapred.AvroWrapper;
import org.apache.avro.mapred.FsInput;
import org.apache.avro.util.Utf8;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.NullWritable;
import org.apache.hadoop.mapred.FileSplit;
import org.apache.hadoop.mapred.InputSplit;
import org.apache.hadoop.mapred.JobConf;
import org.apache.hadoop.mapred.RecordReader;
import org.w3c.dom.DOMException;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

import com.emc.greenplum.gpdb.hadoop.formathandler.util.FormatHandlerUtil;
import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;
import com.emc.greenplum.gpdb.hdfsconnector.ColumnSchema;

public class AvroFileReader {
	int DEFAULT_SPLIT_SIZE = 64*1024*1024;
	
	Configuration conf;
	int segid;
	int totalseg;
	String inputpath;
	
	String schemaFile = null;
	
	Schema schema = null;
	int[] colTypeArray = null;
	
	List<ColumnSchema> tableSchemas = null;

	boolean schemaComplete = false;

	boolean autoSelect = false;

	DocumentBuilderFactory factory=DocumentBuilderFactory.newInstance();
	TransformerFactory tf = TransformerFactory.newInstance();

	OutputStream out = System.out;

	public AvroFileReader(Configuration conf, int segid, int totalseg,String inputpath, List<ColumnSchema> tableSchema,
			String schemaFile, boolean schemaComplete, boolean autoSelect, OutputStream out) {
		this.conf = conf;
		this.segid = segid;
		this.totalseg = totalseg;
		this.inputpath = inputpath;
		this.tableSchemas = tableSchema;
		this.schemaFile = schemaFile;
		this.schemaComplete = schemaComplete;
		this.autoSelect = autoSelect;
		this.out = out;
	}

	/**
	 * Read the input as an AvroInputFormat by using AvroRecordReader
	 * and write it to Std Out
	 * 
	 * @throws Exception when something goes wrong
	 */
	public void readAvroFormat() throws IOException{
		List<Path> toReadFileList = FormatHandlerUtil.getToreadFiles(conf, inputpath);

		if (toReadFileList.size() == 0) {
			throw new IOException("there is no file in the path you given, PATH : " + inputpath);
		}

		Path[] files = toReadFileList.toArray(new Path[toReadFileList.size()]);

		if (schemaFile != null) {
//			if user give us a schema file, read schema from it
			Path avroSchemaPath = new Path(schemaFile);
			readAvroSchema(avroSchemaPath, true);
		}else {
			readAvroSchema(files[0], false);
		}

		if (tableSchemas != null) {
			checkTypeMisMatch(schema, tableSchemas);
		}

		JobConf jconf = new JobConf(conf, AvroFileReader.class);
		GpdbAvroInputFormat.setInputPaths(jconf, files);
		InputSplit[] splits = (new GpdbAvroInputFormat()).getSplits(jconf, totalseg);

		DataOutputStream dos = new DataOutputStream(out);

		AvroWrapper<GenericRecord> avroWrapper = new AvroWrapper<GenericRecord>();

		for (int i = 0; i < splits.length; i++) {
			if (i % totalseg == segid) {
				RecordReader<AvroWrapper<GenericRecord>, NullWritable> reader = new AvroRecordReader(jconf, (FileSplit) splits[i]);

				while (reader.next(avroWrapper, NullWritable.get())) {
					GPDBWritable writable = new GPDBWritable(colTypeArray);

					GenericRecord record = avroWrapper.datum();
					List<Field> fields = schema.getFields();

					for (int j = 0; j < fields.size(); j++) {
						Field field = fields.get(j);
						populateRecord(j ,record, field, null, writable);
					}

					writable.write(dos);
				}

				reader.close();
			}
		}

		dos.close();
	}

	/**
	 * check type mismatch
	 * @param avroSchema, gpdbSchema
	 * @throws IOException 
	 * 
	 * @throws Exception when something goes wrong
	 */
	private void checkTypeMisMatch(Schema avroSchema, List<ColumnSchema> gpdbSchemas) throws IOException {
		List<Field> avroFields = avroSchema.getFields();

		for (int i = 0; i < avroFields.size(); i++) {
			Field avroField = avroFields.get(i);
			ColumnSchema gpdbField = gpdbSchemas.get(i);

			checkFieldTypeMisMatch(i, avroField.schema(), gpdbField);
		}
	}

	private void checkFieldTypeMisMatch(int index, Schema avroSchema, ColumnSchema gpdbField) throws IOException {
		String fieldName = avroSchema.getName();
		
		Schema.Type fieldType = avroSchema.getType();

		if (fieldType == Type.UNION) {
//			we take the first not null type as this column's type
			fieldType = FormatHandlerUtil.firstNotNullType(avroSchema.getTypes());
			avroSchema = FormatHandlerUtil.firstNotNullSchema(avroSchema.getTypes());

			if (fieldType == Type.NULL) {
				throw new IOException(fieldName + " is an union, but only has null type") ;
			}
		}

		switch (fieldType) {
		case RECORD:
			if (!GPDBWritable.isTextOrTextArray(gpdbField.getType()) && !GPDBWritable.isXmlOrXmlArray(gpdbField.getType())) {
				throw new IOException("type mismatch, avro is sub-record but gpdb is not int, gpdbColumnIdex : " + index + ", avroFiled : " + fieldName);
			}
			break;

		case INT:
			if (!GPDBWritable.isSmallIntOrIntOrBigInt(gpdbField.getType())) {
				throw new IOException("type mismatch, avro is int but gpdb is not int, gpdbColumnIdex : " + index + ", avroFiled : " + fieldName);
			}
			break;

		case DOUBLE:
			if (!GPDBWritable.isRealOrDouble(gpdbField.getType())) {
				throw new IOException("type mismatch, avro is double but gpdb is not float, gpdbColumnIdex : " + index + ", avroFiled : " + fieldName);
			}
			break;

		case STRING:
			if (GPDBWritable.isNumberType(gpdbField.getType())) {
				throw new IOException("type mismatch, avro is string but gpdb is a number, gpdbColumnIdex : " + index + ", avroFiled : " + fieldName);
			}
			break;

		case FLOAT:
			if (!GPDBWritable.isRealOrDouble(gpdbField.getType())) {
				throw new IOException("type mismatch, avro is float but gpdb is not real, gpdbColumnIdex : " + index + ", avroFiled : " + fieldName);
			}
			break;

		case LONG:
			if (!GPDBWritable.isIntOrBigInt(gpdbField.getType())) {
				throw new IOException("type mismatch, avro is long but gpdb is not bigint, gpdbColumnIdex : " + index + ", avroFiled : " + fieldName);
			}
			break;

		case FIXED:
		case BYTES:
			if (!GPDBWritable.isByteaOrByteaArray(gpdbField.getType())) {
				throw new IOException("type mismatch, avro is bytes but gpdb is not bytea, gpdbColumnIdex : " + index + ", avroFiled : " + fieldName);
			}
			break;

		case BOOLEAN:
			if (!GPDBWritable.isBoolOrBoolArray(gpdbField.getType())) {
				throw new IOException("type mismatch, avro is boolean but gpdb is not boolean, gpdbColumnIdex : " + index + ", avroFiled : " + fieldName);
			}
			break;

		case ARRAY:
			if (gpdbField.getNdim() == 0 && !GPDBWritable.isXmlOrXmlArray(gpdbField.getType())) {
				throw new IOException("type mismatch, avro is array but gpdb is not array, gpdbColumnIdex : " + index + ", avroFiled : " + fieldName);
			}else {
				checkFieldTypeMisMatch(index, avroSchema.getElementType(), gpdbField);
			}
			break;

		case ENUM:
			if (!GPDBWritable.isIntOrBigInt(gpdbField.getType())) {
				throw new IOException("type mismatch, avro is enum but gpdb is not int, gpdbColumnIdex : " + index + ", avroFiled : " + fieldName);
			}
			break;

		case UNION:
			throw new IOException("you should not be here, internal error");

		case MAP:
		default:
			throw new IOException("unsupported avro data type, " + fieldType);
		}
	}

	/**
	 * decode fields from record to GPDBWritable according to field schema
	 * 
	 * @throws Exception when something goes wrong
	 */
	private void populateRecord(int index, GenericRecord record, Field field, List<Integer> colTypes, GPDBWritable writable) throws IOException {
		String fieldName = field.name();
		Schema fieldSchema = field.schema();
		Schema.Type fieldType = fieldSchema.getType();

		if (writable != null && (record.get(fieldName) == null) ) {
			writable.setNull(index);
			return;
		}

		if (fieldType == Type.UNION) {
//		we take the first not null type as this column's type
			fieldType = FormatHandlerUtil.firstNotNullType(fieldSchema.getTypes());

			if (fieldType == Type.NULL) {
				throw new IOException(fieldName + " is an union, but only has null type") ;
			}
		}

		switch (fieldType) {
		case RECORD:
//			sub record, turn it to xml
			if(colTypes != null) colTypes.add(GPDBWritable.TEXT);
			if(writable != null) writable.setString(index, arrayNestedRecord2xml(record.get(fieldName), field.schema(), fieldName));
			break;

		case INT:
			if(colTypes != null) {
				if (tableSchemas != null && tableSchemas.get(index).getType() == GPDBWritable.SMALLINT) {
					colTypes.add(GPDBWritable.SMALLINT);
				}else {
					colTypes.add(GPDBWritable.INTEGER);
				}
			}
			if(writable != null) {
				if (tableSchemas != null && tableSchemas.get(index).getType() == GPDBWritable.SMALLINT) {
					writable.setShort(index, ((Integer)record.get(fieldName)).shortValue());
				}else {
					writable.setInt(index, (Integer)record.get(fieldName));
				}
			}
			break;

		case DOUBLE:
			if(colTypes != null) colTypes.add(GPDBWritable.FLOAT8);
			if(writable != null) {
				if (record.get(fieldName) instanceof Integer) {
					writable.setDouble(index, Double.valueOf((Integer)record.get(fieldName)));
				}else if (record.get(fieldName) instanceof Long) {
					writable.setDouble(index, Double.valueOf((Long)record.get(fieldName)));
				}else if (record.get(fieldName) instanceof Float) {
					writable.setDouble(index, Double.valueOf((Float)record.get(fieldName)));
				}else {
					writable.setDouble(index, (Double)record.get(fieldName));
				}
			}
			break;

		case STRING:
			if(colTypes != null) colTypes.add(GPDBWritable.VARCHAR);
			if(writable != null) {
				if (record.get(fieldName) instanceof ByteBuffer) {
					writable.setString(index, new String(((ByteBuffer) record.get(fieldName)).array()));
				}else {
					writable.setString(index, ((Utf8) record.get(fieldName)).toString());
				}
			}
			break;

		case FLOAT:
			if(colTypes != null) colTypes.add(GPDBWritable.REAL);
			if(writable != null) {
				if (record.get(fieldName) instanceof Integer) {
					writable.setFloat(index, Float.valueOf((Integer)record.get(fieldName)));
				}else if (record.get(fieldName) instanceof Long) {
					writable.setFloat(index, Float.valueOf((Long)record.get(fieldName)));
				}else {
					writable.setFloat(index, (Float)record.get(fieldName));
				}
			}
			break;

		case LONG:
			if(colTypes != null) colTypes.add(GPDBWritable.BIGINT);
			if(writable != null) {
				if (record.get(fieldName) instanceof Integer) {
					writable.setLong(index, Long.valueOf((Integer)record.get(fieldName)));
				}else {
					writable.setLong(index, (Long)record.get(fieldName));
				}
			}
			break;

		case BYTES:
			if(colTypes != null) colTypes.add(GPDBWritable.BYTEA);
			if(writable != null) {
				if (record.get(fieldName) instanceof Utf8) {
					writable.setBytes(index, (ByteBuffer.wrap(((Utf8) record.get(fieldName)).getBytes())).array());
				}else {
					ByteBuffer bb = (ByteBuffer) record.get(fieldName);
					writable.setBytes(index, (bb).array());
				}
			}
			break;

		case BOOLEAN:
			if(colTypes != null) colTypes.add(GPDBWritable.BOOLEAN);
			if(writable != null) writable.setBoolean(index, (Boolean)record.get(fieldName));
			break;

		case ARRAY:
			if(colTypes != null) colTypes.add(GPDBWritable.TEXT);
			if(writable != null) {
				char delim = tableSchemas == null ? ',' : tableSchemas.get(index).getDelim();
				String arrayString = null;
				if (FormatHandlerUtil.getAvroArrayElementType(fieldSchema) == Type.RECORD) {
//					turn this array[... record ...] to a single xml
					arrayString = arrayNestedRecord2xml(record.get(fieldName), field.schema(), fieldName);
				}else {
					arrayString = FormatHandlerUtil.getArrayString(fieldSchema, record.get(fieldName), true, delim).toString();
				}
				
				writable.setString(index, arrayString);
			}
			break;

		case FIXED:
			if(colTypes != null) colTypes.add(GPDBWritable.BYTEA);
			if(writable != null) {
				Fixed value = (Fixed) record.get(fieldName);
				writable.setBytes(index, value.bytes());
			}
			break;

		case ENUM:
			if(colTypes != null) colTypes.add(GPDBWritable.INTEGER);
			if(writable != null) writable.setInt(index, fieldSchema.getEnumOrdinal(record.get(fieldName).toString()));
			break;

		case UNION:
			throw new IOException("you should not be here, internal error");
		case MAP:
		default:
			throw new IOException("unsupported data type, " + fieldType);
		}
	}

	private String arrayNestedRecord2xml(Object records, Schema fieldSchema, String fieldName) throws IOException {
		try {
			DocumentBuilder db=factory.newDocumentBuilder();
			Document xmldoc = db.newDocument();

			Node node = xmldoc.createElement(fieldName);
			((Element)node).setAttribute("type", fieldSchema.getName());
			
			getSubXml(records, fieldSchema, xmldoc, node, fieldName);

			xmldoc.appendChild(node);

			Transformer transformer = tf.newTransformer();
			DOMSource source = new DOMSource(xmldoc);

			StringWriter sw = new StringWriter();
			StreamResult sResult = new StreamResult(sw);
			transformer.transform(source, sResult);

			return FormatHandlerUtil.trimXmlHeader(sw.getBuffer().toString());
		} catch (ParserConfigurationException e) {
			throw new IOException(e.getMessage());
		} catch (TransformerConfigurationException e) {
			throw new IOException(e.getMessage());
		} catch (TransformerException e) {
			throw new IOException(e.getMessage());
		}
	}

	private void getSubXml(Object records, Schema fieldSchema, Document xmldoc, Node fatherNode, String fieldName) throws DOMException, IOException {
		if (fieldSchema.getType() == Type.UNION) {
			fieldSchema = FormatHandlerUtil.firstNotNullSchema(fieldSchema.getTypes());
		}

		if (fieldSchema.getType() == Type.RECORD) {
//			getRecordXml((GenericRecord)records, fieldSchema, xmldoc, node);
			List<Field> avroFields = fieldSchema.getFields();

			for (int i = 0; i < avroFields.size(); i++) {
				Field avroField = avroFields.get(i);
				Object avroObject = ((GenericRecord)records).get(avroField.name());
				if (avroObject == null) {
					continue;
				}

				Node subNode = xmldoc.createElement(avroField.name());
				((Element)subNode).setAttribute("type", avroField.schema().getName());

				getSubXml(avroObject, avroField.schema(), xmldoc, subNode, avroField.name());

				fatherNode.appendChild(subNode);
			}
		}else if (fieldSchema.getType() == Type.ARRAY) {
//			inner array
			for (int i = 0; i < ((List<Object>)records).size(); i++) {
				Node node = xmldoc.createElement(fieldName);
				((Element)node).setAttribute("type", fieldSchema.getElementType().getName());
				getSubXml(((List<Object>)records).get(i), fieldSchema.getElementType(), xmldoc, node, fieldName);
				fatherNode.appendChild(node);
			}
		}else {
//			array element
			((Element)fatherNode).setAttribute("type", fieldSchema.getName());
			fatherNode.setTextContent(FormatHandlerUtil.formatXmlChar(getElementString(records, fieldSchema, new StringBuilder())).toString());
		}
	}

	private StringBuilder getElementString(Object element, Schema fieldSchema, StringBuilder sb) throws IOException {
		Schema.Type fieldType = fieldSchema.getType();

		if (fieldType == Type.UNION) {
//			we take the first not null type as this column's type
			fieldType = FormatHandlerUtil.firstNotNullType(fieldSchema.getTypes());

			if (fieldType == Type.NULL) {
				throw new IOException(fieldSchema.getName() + " is an union, but only has null type") ;
			}
		}

		switch (fieldType) {
		case INT:
		case DOUBLE:
		case STRING:
		case FLOAT:
		case LONG:
		case BOOLEAN:
			return sb.append(element);

		case BYTES:
			FormatHandlerUtil.byteArray2OctString(((ByteBuffer) element).array(), sb);
			return sb;

		case FIXED:
			FormatHandlerUtil.byteArray2OctString(( (Fixed) element).bytes(), sb);
			return sb;

		case ENUM:
			return sb.append(fieldSchema.getEnumOrdinal(element.toString()));

		case ARRAY:
		case UNION:
			throw new IOException("you should not be here, internal error");
		case MAP:
		default:
			throw new IOException("unsupported data type, " + fieldType);
		}
	}

	/**
	 * read avro schema from the given file in hdfs
	 * 
	 * @throws Exception when something goes wrong
	 */
	private void readAvroSchema(Path filePath, boolean isSchemaFile) throws IOException{
		if (isSchemaFile) {
			FileSystem schemaFs = filePath.getFileSystem(conf);
			schema = new Schema.Parser().parse(schemaFs.open(filePath));
		}else {
			FsInput inStream = new FsInput(filePath, conf);
			DatumReader<GenericRecord> dummyReader = new GenericDatumReader<GenericRecord>();
			DataFileReader<GenericRecord> dummyFileReader = new DataFileReader<GenericRecord>(inStream, dummyReader);
			schema = dummyFileReader.getSchema();
			dummyFileReader.close();
		}

		List<Integer> colTypes = new ArrayList<Integer>();
		List<Schema.Field> fields = schema.getFields();
		for (int i = 0; i < fields.size(); i++) {
			populateRecord(i, null, fields.get(i), colTypes, null);
		}

		colTypeArray = new int[colTypes.size()];
		for (int i = 0; i < colTypes.size(); i++) {
			colTypeArray[i] = colTypes.get(i);
		}
	}
}
