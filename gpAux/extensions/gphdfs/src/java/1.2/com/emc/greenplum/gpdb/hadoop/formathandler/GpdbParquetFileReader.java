package com.emc.greenplum.gpdb.hadoop.formathandler;

import java.io.BufferedOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.StringWriter;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
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

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.Path;
import org.apache.parquet.column.ColumnDescriptor;
import org.apache.parquet.column.page.PageReadStore;
import org.apache.parquet.example.data.Group;
import org.apache.parquet.example.data.simple.convert.GroupRecordConverter;
import org.apache.parquet.format.converter.ParquetMetadataConverter;
import org.apache.parquet.hadoop.ParquetFileReader;
import org.apache.parquet.hadoop.metadata.BlockMetaData;
import org.apache.parquet.hadoop.metadata.ParquetMetadata;
import org.apache.parquet.io.ColumnIOFactory;
import org.apache.parquet.io.MessageColumnIO;
import org.apache.parquet.io.RecordReader;
import org.apache.parquet.schema.GroupType;
import org.apache.parquet.schema.MessageType;
import org.apache.parquet.schema.OriginalType;
import org.apache.parquet.schema.Type;
import org.apache.parquet.schema.Type.Repetition;
import org.w3c.dom.DOMException;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

import com.emc.greenplum.gpdb.hadoop.formathandler.util.FormatHandlerUtil;
import com.emc.greenplum.gpdb.hadoop.io.GPDBWritable;
import com.emc.greenplum.gpdb.hdfsconnector.ColumnSchema;

public class GpdbParquetFileReader {
	private static final String HIVE_SCHEMA_NAME = "hive_schema";

	boolean DATA_TIME_ANNOTATION_ON = false;

	Configuration conf;
	int segid;
	int totalseg;
	String inputpath;

	MessageType schema = null;
	List<ColumnDescriptor> columnDescriptors = null;
	int[] colTypeArray = null;

	List<ColumnSchema> tableSchemas = null;

	boolean schemaComplete = false;

	boolean autoSelect = false;

	boolean isHiveFile = false;

	OutputStream out = System.out;

	DocumentBuilderFactory factory=DocumentBuilderFactory.newInstance();
	TransformerFactory tf = TransformerFactory.newInstance();

	public GpdbParquetFileReader(Configuration conf, int segid, int totalseg,
			String inputpath, List<ColumnSchema> tableSchema, boolean schemaComplete, boolean autoSelect, OutputStream out) {
		this.conf = conf;
		this.segid = segid;
		this.totalseg = totalseg;
		this.inputpath = inputpath;
		this.tableSchemas = tableSchema;
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
	public void readParquetFormat() throws IOException{
		List<FileStatus> toReadFileList = FormatHandlerUtil.getToreadFileStatus(conf, new Path(inputpath));

		if (toReadFileList.size() == 0) {
			throw new IOException("there is no file in the path you given, PATH : " + inputpath);
		}

		Collections.sort(toReadFileList);

		DataOutputStream dos = new DataOutputStream(new BufferedOutputStream(out));

		int counter = 0;
		for (FileStatus toRead : toReadFileList) {
			ParquetMetadata metadata = ParquetFileReader.readFooter(conf, toRead, ParquetMetadataConverter.NO_FILTER);

			if (schema == null) { //we read schema from the first file in the to-read list
				schema = metadata.getFileMetaData().getSchema();
				columnDescriptors = schema.getColumns();
				generateTypeArray(schema.getFields());
				isHiveFile = checkWhetherHive(schema);
				if (tableSchemas != null) {
					checkTypeMisMatch(schema, tableSchemas);
				}
			}

			List<BlockMetaData> toReadBlocks = new ArrayList<BlockMetaData>();

			List<BlockMetaData> blocks = metadata.getBlocks();
			for (int i = 0; i < blocks.size(); i++) {
				if (counter % totalseg == segid) {
					toReadBlocks.add(blocks.get(i));
				}
				counter++;
			}

			ParquetFileReader fileReader = new ParquetFileReader(conf, toRead.getPath(), toReadBlocks, columnDescriptors);

			PageReadStore pages = null;
			while (null != (pages = fileReader.readNextRowGroup())) {
				final long rows = pages.getRowCount();

				MessageColumnIO columnIO = new ColumnIOFactory().getColumnIO(schema);
				RecordReader<Group> recordReader = columnIO.getRecordReader(pages, new GroupRecordConverter(schema));
				for (int line = 0; line < rows; line++) {
					Group g = recordReader.read();

					GPDBWritable writable = new GPDBWritable(colTypeArray);

					List<Type> types = schema.getFields();
					for (int column = 0; column < types.size(); column++) {
						if (g.getFieldRepetitionCount(column) == 0) {
							writable.setNull(column);
						}else {
							generateRecord(column, schema.getFields().get(column), g, writable);
						}
					}

					writable.write(dos);
				}
			}
			dos.flush();

			fileReader.close();
		}

		dos.close();
	}

	private boolean checkWhetherHive(MessageType schema) {
		if (schema.getName() != null && schema.getName().equals(HIVE_SCHEMA_NAME)) {
			return true;
		}
		return false;
	}

	/**
	 * check type mismatch
	 * @param parquetSchema, gpdbSchema
	 * @throws IOException
	 *
	 * @throws Exception when something goes wrong
	 */
	private void checkTypeMisMatch(MessageType parquetSchema, List<ColumnSchema> gpdbSchema) throws IOException {
		List<Type> pqTypes = parquetSchema.getFields();

		for (int i = 0; i < pqTypes.size(); i++) {
			Type pqType = pqTypes.get(i);
			ColumnSchema gpdbType = gpdbSchema.get(i);

//			first rule, array != no-array
			if (pqType.getRepetition() == Repetition.REPEATED && pqType.isPrimitive() && gpdbType.getNdim() == 0) {
				throw new IOException("type mismatch, parquet is array, but gpdb not, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
			}

			if (gpdbType.getNdim() > 0 && pqType.getRepetition() != Repetition.REPEATED) {
				throw new IOException("type mismatch, gpdb is array but parquet not, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
			}

//			second rule, type mismatch
			if (!pqType.isPrimitive()) {
//				it's a group, turn to xml for now
				if (!GPDBWritable.isTextOrTextArray(gpdbType.getType()) && !GPDBWritable.isXmlOrXmlArray(gpdbType.getType())) {
					throw new IOException("type mismatch, parquet is nested group but gpdb is not xml or text, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
				}

				continue;
			}

			OriginalType oType = pqType.getOriginalType();

			switch (pqType.asPrimitiveType().getPrimitiveTypeName()) {
			case BOOLEAN:
				if (!GPDBWritable.isBoolOrBoolArray(gpdbType.getType())) {
					throw new IOException("type mismatch, parquet is boolean but gpdb is not, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
				}
				break;

			case INT32://int32, int_8, int_16, int_32, uint_8, uint_16, uint_32, date, time_millis, decimal
				if (oType == OriginalType.DECIMAL) {
//					transfer to text
					if (!GPDBWritable.isNumericOrNumericArray(gpdbType.getType())) {
						throw new IOException("type mismatch, parquet is decimal but gpdb is not text, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
					}
				}else if ( (oType == OriginalType.DATE && GPDBWritable.isDateOrDateArray(gpdbType.getType()))
						|| (oType == OriginalType.TIME_MILLIS && GPDBWritable.isTimeOrTimeArray(gpdbType.getType()))
						&& DATA_TIME_ANNOTATION_ON) {
					break;
				}else if (!GPDBWritable.isSmallIntOrIntOrBigInt(gpdbType.getType())) {
					throw new IOException("type mismatch, parquet is int32 but gpdb is not, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
				}
				break;

			case INT64://int64, int_64, uint_64, timestamp_millis, decimal
				if (oType == OriginalType.DECIMAL) {
//					transfer to text
					if (!GPDBWritable.isNumericOrNumericArray(gpdbType.getType())) {
						throw new IOException("type mismatch, parquet is decimal but gpdb is not numeric, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
					}
				}else if ( (oType == OriginalType.TIMESTAMP_MILLIS && GPDBWritable.isTimestampOrTimestampArray(gpdbType.getType()))
						&& DATA_TIME_ANNOTATION_ON) {
					break;
				}else if (!GPDBWritable.isIntOrBigInt(gpdbType.getType())) {
					throw new IOException("type mismatch, parquet is int64 but gpdb is not, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
				}
				break;

			case FLOAT:
				if ( !GPDBWritable.isRealOrDouble(gpdbType.getType()) ) {
					throw new IOException("type mismatch, parquet is float but gpdb is not real, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
				}
				break;

			case DOUBLE:
				if (!GPDBWritable.isRealOrDouble(gpdbType.getType())) {
					throw new IOException("type mismatch, parquet is double but gpdb is not float, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
				}
				break;

			case INT96:
			case FIXED_LEN_BYTE_ARRAY://fixed_len_byte_array, decimal
				if ( (oType == OriginalType.INTERVAL && GPDBWritable.isIntervalOrIntervalArray(gpdbType.getType())) 
						&& DATA_TIME_ANNOTATION_ON) {
					break;
				}
			case BINARY://utf8, interval, json, bson, decimal
//				although parquet schema is bytea, but we will see whether user wants a 'text' field
				if (oType == OriginalType.UTF8 || oType == OriginalType.JSON) {
					if (GPDBWritable.isNumberType(gpdbType.getType())) {
						throw new IOException("type mismatch, parquet is text binary but gpdb is not text, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
					}
				}else if (oType == OriginalType.DECIMAL) {
					if (!GPDBWritable.isNumericOrNumericArray(gpdbType.getType())) {
						throw new IOException("type mismatch, parquet is decimal but gpdb is not numeric, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
					}
				}else {
					if ( GPDBWritable.isNumberType(gpdbType.getType()) ) {
						throw new IOException("type mismatch, parquet is byte array but gpdb is number, gpdbColumnIdex : " + i + ", parquetFiled : " + pqType.getName() );
					}
				}
				break;

			default:
				throw new IOException("type not supported by parquet, type:" + pqType.asPrimitiveType().getPrimitiveTypeName());
			}
		}
	}

	/**
	 * decode fields from record to GPDBWritable according to field schema
	 * @param types
	 * @throws IOException
	 *
	 * @throws Exception when something goes wrong
	 */
	private void generateRecord(int index, Type type, Group g, GPDBWritable writable) throws IOException {
		if (!type.isPrimitive()) {
//			this is group, turn it to xml for now, for 'repeated group', we turn it to a single xml, not xml[]
			writable.setString(index, getXmlFromGroup(g, index, type.asGroupType(), type.getRepetition()));
			return;
		}

		if (type.getRepetition() == Repetition.REPEATED) {
//			this is an array, turn it to text
			String arrString = getParquetArrayString(index, type, g);
			writable.setString(index, arrString);
			return;
		}

		OriginalType oType = type.getOriginalType();
		switch (type.asPrimitiveType().getPrimitiveTypeName()) {
		case BOOLEAN:
			writable.setBoolean(index, g.getBoolean(index, 0));
			break;

		case INT32://int32, int_8, int_16, int_32, uint_8, uint_16, uint_32, date, time_millis, decimal
			if (oType == OriginalType.DECIMAL) {
//				transfer to text
				writable.setString(index, "" + g.getInteger(index, 0));
			}else if (oType == OriginalType.DATE && DATA_TIME_ANNOTATION_ON) {
				writable.setString(index, FormatHandlerUtil.buildTimeBasedOnDiff("1970-01-01", "yyyy-mm-dd", Calendar.DAY_OF_YEAR, g.getInteger(index, 0), false));
			}else if (oType == OriginalType.TIME_MILLIS && DATA_TIME_ANNOTATION_ON) {
				writable.setString(index, FormatHandlerUtil.buildTimeBasedOnDiff("00:00:00.000", "hh:mm:ss.SSS", Calendar.MILLISECOND, g.getInteger(index, 0), false));
			}else if (oType == OriginalType.INT_16
					|| (tableSchemas != null && tableSchemas.get(index).getType() == GPDBWritable.SMALLINT)) {
				writable.setShort(index, (short)g.getInteger(index, 0));
			}else {
				writable.setInt(index, g.getInteger(index, 0));
			}
			break;

		case INT64://int64, int_64, uint_64, timestamp_millis, decimal
			if (oType == OriginalType.DECIMAL) {
				writable.setString(index, "" + g.getLong(index, 0));
			}else if (oType == OriginalType.TIMESTAMP_MILLIS && DATA_TIME_ANNOTATION_ON) {
				writable.setString(index, FormatHandlerUtil.buildTimeBasedOnDiff("1970-01-01 00:00:00.000", "yyyy-mm-dd hh:mm:ss.SSS", Calendar.MILLISECOND, g.getLong(index, 0), true));
			}else {
				writable.setLong(index, g.getLong(index, 0));
			}
			break;

		case FLOAT:
			writable.setFloat(index, g.getFloat(index, 0));
			break;

		case DOUBLE:
			writable.setDouble(index, g.getDouble(index, 0));
			break;

		case INT96://timestamp in hive
			writable.setBytes(index, g.getInt96(index, 0).getBytes());
			break;

		case FIXED_LEN_BYTE_ARRAY://fixed_len_byte_array, decimal, interval, decimal in hive
			if (isHiveFile && oType == OriginalType.DECIMAL) {
				int scale = type.asPrimitiveType().getDecimalMetadata().getScale();
				BigDecimal bd = new BigDecimal(new BigInteger(g.getBinary(index, 0).getBytes()), scale);
				writable.setString(index, bd.toString());
				break;
			}
		case BINARY://utf8, json, bson, decimal
//			although parquet schema is bytea, but we will see whether user wants a 'text' field
			if (oType == OriginalType.UTF8 || oType == OriginalType.JSON || oType == OriginalType.DECIMAL
				|| (tableSchemas != null && tableSchemas.get(index).getType() == GPDBWritable.TEXT)) {
				writable.setString(index, new String(g.getBinary(index, 0).getBytes(), "utf8"));
			}else if (oType == OriginalType.INTERVAL && DATA_TIME_ANNOTATION_ON) {
				writable.setString(index, FormatHandlerUtil.buildParquetInterval(g.getBinary(index, 0).getBytes()));
			}else {
				writable.setBytes(index, g.getBinary(index, 0).getBytes());
			}
			break;

		default:
			throw new IOException("type not supported by parquet, type:" + type.asPrimitiveType().getPrimitiveTypeName());
		}
	}

	private String getXmlFromGroup(Group group, int fieldIndex, GroupType groupType, Repetition repetition) throws IOException {
		int repeateCount = group.getFieldRepetitionCount(fieldIndex);
		if (repeateCount == 0) {
			return null;
		}

		try {
			DocumentBuilder db=factory.newDocumentBuilder();
			Document xmldoc = db.newDocument();

			Node root = xmldoc.createElement(groupType.getName());
			if (repetition == Repetition.REPEATED) {
				((Element)root).setAttribute("type", "repeated");
			}else {
				((Element)root).setAttribute("type", "group");
			}

			Node fatherNode = root;

			for (int i = 0; i < repeateCount; i++) {
				if (repetition == Repetition.REPEATED) {
					fatherNode = xmldoc.createElement(groupType.getName());
					((Element)fatherNode).setAttribute("type", "group");
				}

				getXml(group.getGroup(fieldIndex, i), groupType, xmldoc, fatherNode);

				if (repetition == Repetition.REPEATED) {
					root.appendChild(fatherNode);
				}
			}

			xmldoc.appendChild(root);

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

	private void getXml(Group group, GroupType groupType, Document xmldoc, Node fatherNode) throws DOMException, IOException {
		List<Type> types = groupType.getFields();
		for (int i = 0; i < types.size(); i ++) {
			Type type = types.get(i);
			int eleNum = 1;

			Node newFatherNode = fatherNode;
			if (type.isRepetition(Repetition.REPEATED)) {
				eleNum = group.getFieldRepetitionCount(i);

				newFatherNode = xmldoc.createElement(type.getName());
				((Element)newFatherNode).setAttribute("type", "repeated");
			}

			for (int j = 0; j < eleNum; j++) {
				Node eleNode = xmldoc.createElement(type.getName());
				if (!type.isPrimitive()) {
//					sub group
					Group subGroup = group.getGroup(i, j);
					getXml(subGroup, type.asGroupType(), xmldoc, eleNode);
					((Element)eleNode).setAttribute("type", "group");
				}else {
					eleNode.setTextContent(getParquetElementString(i, j, types.get(i), group));
					((Element)eleNode).setAttribute("type", type.asPrimitiveType().getPrimitiveTypeName().name().toLowerCase());
					OriginalType oType = type.getOriginalType();
					if (oType != null) {
						((Element)eleNode).setAttribute("anotation", oType.name());
					}
				}
				newFatherNode.appendChild(eleNode);
			}

			if (type.isRepetition(Repetition.REPEATED)) {
				fatherNode.appendChild(newFatherNode);
			}
		}
	}

	private String getParquetArrayString(int index,Type type, Group g) throws IOException {
		StringBuilder sb = new StringBuilder();

		sb.append("{");
		for (int i = 0; i < g.getFieldRepetitionCount(index); i++) {
			if (i > 0) {
				char delim = (tableSchemas == null) ? ',': tableSchemas.get(index).getDelim();
				sb.append(delim);
			}
			getParquetElementString(index, i, type, g, sb);
		}
		sb.append("}");

		return sb.toString();
	}

	private String getParquetElementString(int fieldIndex, int elementIndex, Type type, Group group) throws IOException{
		StringBuilder sb = new StringBuilder();
		getParquetElementString(fieldIndex, elementIndex, type, group, sb);
		return FormatHandlerUtil.formatXmlChar(sb).toString();
	}

	private void getParquetElementString(int fieldIndex, int elementIndex, Type type, Group g, StringBuilder sb) throws IOException {
		OriginalType oType = type.getOriginalType();
		switch (type.asPrimitiveType().getPrimitiveTypeName()) {
		case BOOLEAN:
		case INT32:
			if (oType == OriginalType.DATE && DATA_TIME_ANNOTATION_ON) {
				sb.append( FormatHandlerUtil.buildTimeBasedOnDiff("1970-01-01", "yyyy-mm-dd", Calendar.DAY_OF_YEAR, g.getInteger(fieldIndex, elementIndex), false) );
				break;
			}else if (oType == OriginalType.TIME_MILLIS && DATA_TIME_ANNOTATION_ON) {
				sb.append( FormatHandlerUtil.buildTimeBasedOnDiff("00:00:00.000", "hh:mm:ss.SSS", Calendar.MILLISECOND, g.getInteger(fieldIndex, elementIndex), false) );
				break;
			}

		case INT64:
			if (oType == OriginalType.DATE && DATA_TIME_ANNOTATION_ON) {
				sb.append( FormatHandlerUtil.buildTimeBasedOnDiff("1970-01-01 00:00:00.000", "yyyy-mm-dd hh:mm:ss.SSS", Calendar.MILLISECOND, g.getLong(fieldIndex, elementIndex), true) );
				break;
			}

		case FLOAT:
		case DOUBLE:
			sb.append( g.getValueToString(fieldIndex, elementIndex) );
			break;

		case INT96:
			FormatHandlerUtil.byteArray2OctString(g.getInt96(fieldIndex, elementIndex).getBytes(), sb);
			break;

		case FIXED_LEN_BYTE_ARRAY:
			if (oType == OriginalType.INTERVAL && DATA_TIME_ANNOTATION_ON) {
				sb.append( FormatHandlerUtil.buildParquetInterval(g.getBinary(fieldIndex, elementIndex).getBytes()) );
				break;
			}

		case BINARY:
			if (oType == OriginalType.UTF8 || oType == OriginalType.DECIMAL || oType == OriginalType.JSON
					|| (tableSchemas != null && tableSchemas.get(fieldIndex).getType() == GPDBWritable.TEXT)) {
				sb.append( g.getValueToString(fieldIndex, elementIndex) );
			}else {
				FormatHandlerUtil.byteArray2OctString(g.getBinary(fieldIndex, elementIndex).getBytes(), sb);
			}
			break;

		default:
			throw new IOException("type not supported by parquet, type:" + type.asPrimitiveType().getPrimitiveTypeName());
		}
	}

	/**
	 * generate type array according to the types in schema
	 * @param types
	 * @throws IOException
	 *
	 * @throws Exception when something goes wrong
	 */
	private void generateTypeArray(List<Type> types) throws IOException {
		colTypeArray = new int[types.size()];
		for (int i = 0; i < types.size(); i++) {
			Type type= types.get(i);
			if (type.getRepetition() == Repetition.REPEATED) {
//				this is an array, turn it to text
				colTypeArray[i] = GPDBWritable.TEXT;

				continue;
			}

			if (!type.isPrimitive()) {
//				this is subgroup, turn it to text
				colTypeArray[i] = GPDBWritable.TEXT;

				continue;
			}

			OriginalType oType = type.getOriginalType();

			switch (type.asPrimitiveType().getPrimitiveTypeName()) {
			case BOOLEAN:
				colTypeArray[i] = GPDBWritable.BOOLEAN;
				break;

			case INT32://int32, int_8, int_16, int_32, uint_8, uint_16, uint_32, date?, time_millis?, decimal
				if (oType == OriginalType.DECIMAL) {
					colTypeArray[i] = GPDBWritable.TEXT;
				}else if ( (oType == OriginalType.DATE || oType == OriginalType.TIME_MILLIS) && DATA_TIME_ANNOTATION_ON) {
					colTypeArray[i] = GPDBWritable.TEXT;
				}else if (oType == OriginalType.INT_16
						|| (tableSchemas != null && tableSchemas.get(i).getType() == GPDBWritable.SMALLINT)) {
					colTypeArray[i] = GPDBWritable.SMALLINT;
				}else {
					colTypeArray[i] = GPDBWritable.INTEGER;
				}

				break;

			case INT64://int64, int_64, uint_64, timestamp_millis?, decimal
				if (oType == OriginalType.DECIMAL) {
					colTypeArray[i] = GPDBWritable.TEXT;
				}else if (oType == OriginalType.TIMESTAMP_MILLIS && DATA_TIME_ANNOTATION_ON) {
					colTypeArray[i] = GPDBWritable.TEXT;
				}else {
					colTypeArray[i] = GPDBWritable.BIGINT;
				}
				break;

			case FLOAT:
				colTypeArray[i] = GPDBWritable.REAL;
				break;

			case DOUBLE:
				colTypeArray[i] = GPDBWritable.FLOAT8;
				break;

			case INT96:
				colTypeArray[i] = GPDBWritable.BYTEA;
				break;

			case FIXED_LEN_BYTE_ARRAY://fixed_len_byte_array, decimal, interval?
				if (oType == OriginalType.DECIMAL) {
					colTypeArray[i] = GPDBWritable.TEXT;
				}else if (oType == OriginalType.INTERVAL && DATA_TIME_ANNOTATION_ON) {
					colTypeArray[i] = GPDBWritable.TEXT;
				}else {
					colTypeArray[i] = GPDBWritable.BYTEA;
				}
				break;

			case BINARY://utf8, interval, json, bson, decimal
				if (oType == OriginalType.UTF8 || oType == OriginalType.DECIMAL || oType == OriginalType.JSON
					|| (tableSchemas != null && tableSchemas.get(i).getType() == GPDBWritable.TEXT)) {
					colTypeArray[i] = GPDBWritable.TEXT;
				}else {
					colTypeArray[i] = GPDBWritable.BYTEA;
				}
				break;

			default:
				throw new IOException("type not supported by parquet, type:" + type.asPrimitiveType().getPrimitiveTypeName());
			}
		}
	}
}
