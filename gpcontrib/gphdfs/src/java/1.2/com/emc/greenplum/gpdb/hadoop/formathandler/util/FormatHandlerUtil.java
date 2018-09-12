package com.emc.greenplum.gpdb.hadoop.formathandler.util;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.apache.avro.Schema;
import org.apache.avro.Schema.Type;
import org.apache.avro.generic.GenericData.Fixed;
import org.apache.avro.util.Utf8;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.LocatedFileStatus;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.RemoteIterator;

/**
 * helper class for handler
 */
public class FormatHandlerUtil {
	static String intervalPostgresPatternString = "(\\+?([\\d-]+) years? )?(\\+?([\\d-]+) mons? )?(\\+?([\\d-]+) days? )?(\\+?([\\d-]+):(\\d+):([\\d\\.]*))?";
	static int yearGoupIndex = 2;
	static int monthGroupIndex = 4;
	static int dayGroupIndex = 6;
	static int hourGroupIndex = 8;
	static int minGroupIndex = 9;
	static int secGroupIndex = 10;
	static Pattern intervalPostgresParren = Pattern.compile(intervalPostgresPatternString);

	/**
	 * get all the files needed to read, if inputpath is an file, just itself
	 *
	 * @throws Exception when something goes wrong
	 */
	public static List<Path> getToreadFiles(Configuration conf, String inputpath) throws IOException {
		Path input = new Path(inputpath);
		return getToreadFiles(conf, input);
	}

	/**
	 * get all the files needed to read, if inputpath is an file, just itself
	 *
	 * @throws Exception when something goes wrong
	 */
	public static List<Path> getToreadFiles(Configuration conf, Path input) throws IOException {
		List<Path> filelList = new ArrayList<Path>();

		FileSystem fsystem = input.getFileSystem(conf);

		FileStatus[] fStatus = fsystem.globStatus(input);
		if (fStatus != null) {
			for (FileStatus fileStatus : fStatus) {
				if (fileStatus.isDirectory()) {
					RemoteIterator<LocatedFileStatus> lfsIterator = fsystem.listFiles(fileStatus.getPath(), true);
					while (lfsIterator.hasNext()) {
						LocatedFileStatus fs = (LocatedFileStatus) lfsIterator.next();

						filelList.add(fs.getPath());
					}
				}else {
					filelList.add(fileStatus.getPath());
				}
			}
		}

		return filelList;
	}

	/**
	 * get all the files needed to read, if inputpath is an file, just itself
	 * @throws IOException
	 *
	 * @throws Exception when something goes wrong
	 */
	public static List<FileStatus> getToreadFileStatus(Configuration conf, Path input) throws IOException {
		List<FileStatus> fList = new ArrayList<FileStatus>();

		FileSystem fsystem = input.getFileSystem(conf);
		FileStatus[] fStatus = fsystem.globStatus(input);

		if (fStatus != null) {
			for (FileStatus fileStatus : fStatus) {
				if (fileStatus.isDirectory()) {
					fList.addAll(getAllSubFileStatus(fsystem, fileStatus.getPath()));
				}else {
					fList.add(fileStatus);
				}
			}
		}

		return fList;
	}

	/**
	 * get all sub files under a directory
	 */
	private static List<FileStatus> getAllSubFileStatus(FileSystem fsSystem, Path path) throws FileNotFoundException, IOException {
		List<FileStatus> fList = new ArrayList<FileStatus>();

		for (FileStatus fileStatus : fsSystem.listStatus(path)) {
			if (fileStatus.isDirectory()) {
				fList.addAll(getAllSubFileStatus(fsSystem, fileStatus.getPath()));
			}else {
				fList.add(fileStatus);
			}
		}

		return fList;
	}

	/**
	 * trim xml header
	 */
	public static String trimXmlHeader(String xml) {
		int contextStart = 0;
		for (int i = 0; i < xml.length(); i++) {
			if (xml.charAt(i) == '>') {
				contextStart = i + 1;
				break;
			}
		}
		return xml.substring(contextStart);
	}

	/**
	 * format xml chars
	 */
	public static StringBuilder formatXmlChar(StringBuilder sb) {
		for (char i = 0; i < sb.length(); i++) {
			switch (i) {
			case '"':
				sb.replace(i, i+1, "&quot;");
				break;
			case '\'':
				sb.replace(i, i+1, "&apos;");
				break;
			case '<':
				sb.replace(i, i+1, "&lt;");
				break;
			case '>':
				sb.replace(i, i+1, "&gt;");
				break;
			case '&':
				sb.replace(i, i+1, "&amp;");
				 break;
			default:
				break;
			}
		}

		return sb;
	}

	/**
	 * get array element type, for avro
	 */
	public static Type getAvroArrayElementType(Schema arraySchema) {
		if (arraySchema.getType() == Type.UNION) {
			arraySchema = FormatHandlerUtil.firstNotNullSchema(arraySchema.getTypes());
		}

		if (arraySchema.getElementType().getType() == Type.ARRAY) {
			return getAvroArrayElementType(arraySchema.getElementType());
		}else {
			return arraySchema.getElementType().getType();
		}
	}


	/**
	 * get time diff between start and end using format
	 * * @throws IOException 
	 */
	public static String buildTimeBasedOnDiff(String start, String format, int calendarUnit, long diff, boolean isTimeStamp) throws IOException{
		try {
			SimpleDateFormat dateFormat = new SimpleDateFormat(format);
			Calendar begin = Calendar.getInstance();
			begin.setTime(dateFormat.parse(start));
			if (isTimeStamp) {
				begin.setTimeInMillis(diff);
			}else {
				begin.add(calendarUnit, (int)diff);
			}

			return dateFormat.format(begin.getTime());
		} catch (ParseException e) {
			throw new IOException("can't parse date, start:" + start);
		}
	}

	/**
	 * get time diff between start and end using format
	 * @throws IOException 
	 */
	public static long getTimeDiff(String start, String end, String format, int timeDiff) throws IOException{
		try {
			SimpleDateFormat dateFormat = new SimpleDateFormat(format);
			Date d1 = dateFormat.parse(start);
			Date d2 = dateFormat.parse(end);
			return ((d1.getTime()-d2.getTime())/timeDiff);
		} catch (ParseException e) {
			throw new IOException("can't parse date, start:" + start +" end:" + end);
		}
	}

	/**
	 * for avro union
	 */
	public static Type firstNotNullType(List<Schema> types) {
		for (Schema schema : types) {
			if (schema.getType() != Type.NULL) {
				return schema.getType();
			}
		}
		return Type.NULL;
	}

	/**
	 * for avro union
	 */
	public static Schema firstNotNullSchema(List<Schema> types) {
		for (Schema schema : types) {
			if (schema.getType() != Type.NULL) {
				return schema;
			}
		}
		return null;
	}

	/**
	 * build interval format for parquet
	 */
	public static String buildParquetInterval(byte[] diff) {
		int totalMonth = 0, totalDay = 0, totalMillSec = 0;
		int mod = 16 * 16;

		for (int i = 0; i < 4; i++) {
			totalMonth += totalMonth * mod + diff[i];
		}

		for (int i = 4; i < 8; i++) {
			totalDay += totalDay * mod + diff[i];
		}

		for (int i = 8; i < 12; i++) {
			totalMillSec += totalMillSec * mod + diff[i];
		}

		int hour = totalMillSec/1000/3600;
		int min = (totalMillSec - hour*3600*1000)/1000/60;
		double sec = ((double)(totalMillSec - hour*3600*1000 - min*60*1000))/1000;
		return totalMonth/12 + "year" + totalMonth%12 + " mons " + totalDay + "days " + hour + ":" + min + ":" + sec;
	}

	/**
	 * for avro union
	 * @throws IOException 
	 */
	public static byte[] getParquetInterval(String string) throws IOException {
		Matcher m = intervalPostgresParren.matcher(string);
		if (m.find()) {
			int year=0, mon=0, day=0, hour=0, min=0;
			double sec = 0.0;

			if (m.group(2) != null) {
				year = Integer.parseInt(m.group(yearGoupIndex));
			}

			if (m.group(monthGroupIndex) != null) {
				mon = Integer.parseInt(m.group(monthGroupIndex));
			}

			if (m.group(dayGroupIndex) != null) {
				day = Integer.parseInt(m.group(6));
			}

			if (m.group(hourGroupIndex) != null) {
				hour = Integer.parseInt(m.group(hourGroupIndex));
			}

			if (m.group(minGroupIndex) != null) {
				min = Integer.parseInt(m.group(minGroupIndex));
			}

			if (m.group(secGroupIndex) != null) {
				sec = Double.parseDouble(m.group(secGroupIndex));
			}

			byte[] interval = new byte[12];

			int totalMonth = year*12 + mon;
			int mod = 16 * 16;
			for (int i = 0; i < 4; i++) {
				interval[i] = (byte) (totalMonth % mod);
				totalMonth = totalMonth >> 8;
			}

			int totalDay = day;
			for (int i = 4; i < 8; i++) {
				interval[i] = (byte) (totalDay % mod);
				totalDay = totalDay >> 8;
			}

			long totalMillSec = (long) ((hour*3600 + min*60 +sec) * 1000);
			for (int i = 8; i < 12; i++) {
				interval[i] = (byte) (totalMillSec % mod);
				totalMillSec = totalMillSec >> 8;
			}

			return interval;
		}else {
			throw new IOException("interval pattern didn't match postgres stype, string:" + string);
		}
	}

	/**
	 * for read, decode array according to schema
	 * @param delim
	 */
	public static StringBuilder getArrayString(Schema elementSchema, Object arrayObject, boolean isTopLevel, char delim) throws IOException {
		if (elementSchema.getType() == Type.ARRAY) {
			if (arrayObject == null) {
				if (isTopLevel) {
					return null;
				}else {
					return new StringBuilder("NULL");
				}
			}

			StringBuilder sb = new StringBuilder();
			sb.append('{');
			List<Object> oList = (List<Object>) arrayObject;
			for (int i = 0; i < oList.size(); i++) {
				if (i > 0) {
					sb.append(delim);
				}
				sb.append(getArrayString(elementSchema.getElementType(), oList.get(i), false, delim));
			}
			sb.append("}");

			return sb;
		}else {
			return getElementString(elementSchema, arrayObject, isTopLevel, delim);
		}
	}

	/**
	 * get element string according to schema
	 */
	private static StringBuilder getElementString(Schema schema, Object arrayObject, boolean isTopLevel, char delim) throws IOException {
		StringBuilder sb = new StringBuilder();

		Type type = schema.getType();
		switch (type) {
		case ARRAY:
			return getArrayString(schema, arrayObject, isTopLevel, delim);

		case BYTES:
			byte[] ba = ((ByteBuffer)arrayObject).array();
			byteArray2OctString(ba, sb);
			break;

		case STRING:
			Utf8 tString = (Utf8) arrayObject;

			boolean unQuoted = false;
			if (tString.charAt(0) != '\"') {
				unQuoted = true;
			}

			if (unQuoted) {
				sb.append('"');
			}

			sb.append(arrayObject);

			if (unQuoted) {
				sb.append('"');
			}

			break;

		case FIXED:
			byte[] fba = ((Fixed) arrayObject).bytes();
			byteArray2OctString(fba, sb);
			break;

		case UNION:
//			deal with union{null, some other type}
//			Type fieldType = firstNotNullType(schema.getTypes());
			schema = firstNotNullSchema(schema.getTypes());

			if (schema == null) {
				throw new IOException(schema + " is an union, but only has null type") ;
			}

			if (arrayObject == null) {
//				this is a null element
				sb.append("NULL");
			}else {
				sb = getElementString(schema, arrayObject, isTopLevel, delim);
			}
			break;

		case ENUM:
			sb.append(schema.getEnumOrdinal(arrayObject.toString()));
			break;

		default:
			sb.append(arrayObject.toString());
			break;
		}
		return sb;
	}

	/**
	 * for write, decode array value according to schema
	 */
	public static Object decodeString(Schema schema, String value, boolean isTopLevel, char delimiter) throws IOException {
		if (schema.getType() == Type.ARRAY) {
			if (value == null) {
//				null array
				return null;
			}

			List<Object> list = new ArrayList<Object>();
			String[] splits = getArraySplits(value.toCharArray(), delimiter);
			for (String string : splits) {
				list.add(decodeString(schema.getElementType(), string, false, delimiter));
			}
			return list;
		}else {
			if (schema.getType() == Type.UNION) {
//			we take the first not null type as this column's type
				schema = FormatHandlerUtil.firstNotNullSchema(schema.getTypes());

				if (schema == null) {
					throw new IOException(schema.getName() + " is an union, but only has null type") ;
				}

				if (schema.getType() == Type.ARRAY) {
					return decodeString(schema, value, isTopLevel, delimiter);
				}
			}

			if (value != null && value.equals("NULL") && !isTopLevel) {
//				NULL in an array
				return null;
			}

			Type fieldType = schema.getType();
			switch (fieldType) {
			case INT:
				return Integer.parseInt(value);
			case DOUBLE:
				return Double.parseDouble(value);
			case STRING:
				return value;
			case FLOAT:
				return Float.parseFloat(value);
			case LONG:
				return Long.parseLong(value);
			case BYTES:
				return octString2byteArray(value);
			case BOOLEAN:
				return Boolean.parseBoolean(value);
			case ENUM:
			case MAP:
			case FIXED:
//		we don't support these types now, because they are not in gpdb
			default:
				throw new IOException("type :" + schema.getType() +" can't be support now");
			}
		}
	}

	/**
	 * decode oct string in byte[] in gpdb to byte array, e.g ""\\\\001\\\\002""
	 */
	@Deprecated
	private static ByteBuffer pureOctString2byteArray(String value) {
		List<Byte> bList = new ArrayList<Byte>();
		String[] splits = value.substring(1, value.length() - 1).split("\\\\\\\\");
		for (String string : splits) {
			if (string.length() != 0) {
				byte b = octString2Byte(string);
				bList.add(b);
			}
		}

		byte[] ba = new byte[bList.size()];
		for (int i = 0; i < bList.size(); i++) {
			ba[i] = bList.get(i);
		}

		return ByteBuffer.wrap(ba);
	}

	/**
	 * decode oct string in byte[] in gpdb to byte array, according postgres escaped encode
	 * e.g ""\\\\001\\\\002"", "\"abcdefg", "'hij\"klmn"
	 */
	public static ByteBuffer octString2byteArray(String value) {
		List<Byte> bList = new ArrayList<Byte>();

		int index = 0, length = value.length();
		if (value.charAt(0) == '"') {
			index = 1;
			length -= 1;
		}

		while (index < length) {
			if (value.charAt(index) == '\\' && value.charAt(index + 1) == '\\') {
				index += 2;
				if (value.charAt(index) == '\\' && value.charAt(index + 1) == '\\') {
					bList.add((byte) 0134);
					index += 2;
				}else {
					bList.add(octString2Byte(value.substring(index, index + 3)));
					index += 3;
				}
			}else if (value.charAt(index) == '\\') {
				bList.add((byte) value.charAt(index+1));
				index += 2;
			}else {
				bList.add((byte) value.charAt(index));
				index++;
			}
		}

		byte[] ba = new byte[bList.size()];
		for (int i = 0; i < bList.size(); i++) {
			ba[i] = bList.get(i);
		}

		return ByteBuffer.wrap(ba);
	}

	/**
	 * decode oct string to byte
	 */
	private static byte octString2Byte(String string) {
		int num = 0;
		for (int i = 0; i < string.length(); i++) {
			num *= 8;
			num += (string.charAt(i) - '0');
		}
		return (byte) num;
	}

	/**
	 * split top level array to elements
	 * @throws IOException
	 */
	public static String[] getArraySplits(char[] value, char delimiter) throws IOException {
		List<Integer> posList = new ArrayList<Integer>();
		posList.add(0);

		if (value[0] != '{' || value[value.length - 1] != '}') {
			throw new IOException("array dim mismatch, rawData : " + new String(value));
		}

		int depth = 0;
		boolean inQuoted = false; //in case: {"a,b", "c{d"}
		for (int i = 0; i < value.length; i++) {
			if (value[i] == delimiter) {
				if (depth == 1 && !inQuoted) {
					posList.add(i);
				}

				continue;
			}

			switch (value[i]) {
			case '{':
				if (!inQuoted) {
					depth++;
				}
				break;
			case '}':
				if (!inQuoted) {
					depth--;
					if (depth == 0) {
//						the most out border
						posList.add(i);
					}
				}
				break;
			case '\"':
				if (isQuote(value, i)) {
					inQuoted = !inQuoted;
				}

				break;

			default:
				break;
			}
		}

		String[] subStrings = new String[posList.size() - 1];
		for (int i = 0; i < posList.size() - 1; i++) {
			subStrings[i] = new String(value, posList.get(i) + 1, posList.get(i+1) - posList.get(i) - 1);
		}

		return subStrings;
	}

	/**
	 * fix c\"d
	 */
	private static boolean isQuote(char[] value, int index) {
		int num = 0;
		for (int i = index - 1; i >= 0; i--) {
			if (value[i] == '\\') {
				num++;
			}else {
				break;
			}
		}
		return num%2 == 0;
	}

	/**
	 * decode byte array object to an oct string
	 */
	public static void byteArray2OctString(byte[] fba, StringBuilder sb) {
		sb.append('"');
		for (int j = 0; j < fba.length; j++) {
			sb.append(byte2OctString(fba[j]));
		}
		sb.append('"');
	}

	/**
	 * get array string
	 */
	public static String getArrayString(Type type, Object arrayObject) throws UnsupportedEncodingException {
		if (type == Type.BYTES) {
			StringBuilder sb = new StringBuilder();
			sb.append('{');

			List<ByteBuffer> barr = (List<ByteBuffer>) arrayObject;
			for (int i = 0; i < barr.size(); i++) {
				if (i > 0) {
					sb.append(',');
				}

				sb.append('"');

				byte[] bb = barr.get(i).array();
				for (int j = 0; j < bb.length; j++) {
					sb.append(byte2OctString(bb[j]));
				}

				sb.append('"');
			}

			sb.append('}');

			return sb.toString();
		}else {
			return getArrayString(arrayObject.toString());
		}
	}

	/**
	 * byte 2 oct string
	 */
	private static Object byte2OctString(byte b) {
		StringBuilder sBuffer = new StringBuilder();
		sBuffer.append('\\').append('\\');

		sBuffer.append(byte2oct(b));

		return sBuffer.toString();
	}

	/**
	 * byte to oct
	 */
	private static String byte2oct(byte b) {
		int num = hex2ten(b);
		char[] na = new char[3];
		for (int i = 2; i >=0; i--) {
			na[i] = (char) ('0' + num%8);
			num /= 8;
		}
		return new String(na);
	}

	/**
	 * decode a number from hex to ten
	 */
	private static int hex2ten(byte b) {
		int num = 0;
		for (int i = 0; i < 8; i++) {
			int t = (b & 0x01);
			if (i == 0) {
				num += t;
			}else{
				num += t * (2 << (i-1));
			}

			b >>= 1;
		}
		return num;
	}

	/**
	 * get array string
	 */
	private static String getArrayString(String arrayString) {
		char[] arr = arrayString.toCharArray();
		arr[0] = '{';
		arr[arr.length - 1] = '}';
		return new String(arr);
	}

	/**
	 * utf8 list to string
	 */
	private static String utf8List2String(List<Utf8> list) {
		StringBuilder sb = new StringBuilder();
		sb.append("{ ");
		for (int i = 0; i < list.size(); i++) {
			if (i > 0) {
				sb.append(",");
			}
			sb.append("\"").append(list.get(i).toString()).append("\" ");
		}
		sb.append(" }");

		return sb.toString();
	}
}
