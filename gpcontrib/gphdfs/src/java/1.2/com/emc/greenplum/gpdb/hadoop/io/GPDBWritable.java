package com.emc.greenplum.gpdb.hadoop.io;

import java.io.IOException;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInput;
import java.io.DataOutput;
import java.io.DataOutputStream;
import java.io.DataInputStream;

import org.apache.hadoop.io.Writable;

import com.emc.greenplum.gpdb.hdfsconnector.HDFSWriter;


/**
 * This class represents a GPDB record in the form of
 * a Java object.
 * @author achoi
 *
 */
public class GPDBWritable implements Writable {
	/*
	 * GPDBWritable format:
	 * table schema format:
	 * Total Length | Version | table name len | table name | #columns | { col name len | col name | col type | not null| dims | delimeter}
	 * 4 byte       | 2 byte    4 byte             var len     4 byte      4 byte         var len    1 byte      1 byte  4 byte   1 byte         
	 * 
	 * data is using the following serialization form:
	 * Total Length | Version | #columns | Col type | Col type |... | Null Bit array   | Col val...
     * 4 byte		| 2 byte	2 byte	   1 byte     1 byte	      ceil(#col/8) byte	 fixed length or var len
     * 
     * For fixed length type, we know the length.
     * In the col val, we align pad according to the alignemnt requirement of the type.
     * For var length type, the alignment is always 4 byte.
     * For var legnth type, col val is <4 byte length><payload val>
	 */


	/*
	 * "DEFINE" of the Database Datatype OID
	 * All datatypes are supported. The types listed here
	 * are commonly used type to facilitaes ease-of-use.
	 * Note that String should always be UTF-8 formatted.
	 */
	public static final int BOOLEAN   =   16;
	public static final int BYTEA     =   17;
	public static final int CHAR      =   18;
	public static final int BIGINT    =   20;
	public static final int SMALLINT  =   21;
	public static final int INTEGER   =   23;
	public static final int TEXT      =   25;
	public static final int REAL      =  700;
	public static final int FLOAT8    =  701;
	public static final int BPCHAR    = 1042;
	public static final int VARCHAR   = 1043;
	public static final int DATE      = 1082;
	public static final int TIME      = 1083;
	public static final int TIMESTAMP = 1114;
	public static final int NUMERIC   = 1700;

	/*
	 * new added types
	 */
	public static final int INTERVAL = 1186;
	public static final int XML = 142;

	public static final int BOOLEAN_ARR   = 1000;
	public static final int BYTEA_ARR   = 1001;
	public static final int CHAR_ARR   = 1002;
	public static final int NAME_ARR   = 1003;
	public static final int INT8_ARR   = 1016;
	public static final int INT2_ARR   = 1005;
	public static final int INT2VECTOR_ARR   = 1006;
	public static final int INT4_ARR   = 1007;
	public static final int REGPROC_ARR   = 1008;
	public static final int TEXT_ARR   = 1009;
	public static final int OID_ARR   = 1028;
	public static final int TID_ARR   = 1010;
	public static final int XID_ARR   = 1011;
	public static final int CID_ARR   = 1012;
	public static final int OIDVECTOR_ARR   = 1013;
	public static final int XML_ARR   = 143;
	public static final int POINT_ARR   = 1013;
	public static final int LSSEG_ARR   = 1018;
	public static final int PATH_ARR   = 1019;
	public static final int BOX_ARR   = 1020;
	public static final int POLYGON_ARR   = 1027;
	public static final int LINE_ARR   = 629;
	public static final int FLOAT4_ARR   = 1021;
	public static final int FLOAT8_ARR   = 1022;
	public static final int ABSTIME_ARR   = 1023;
	public static final int RELTIME_ARR   = 1024;
	public static final int TINTERVAL_ARR   = 1025;
	public static final int CIRCLE_ARR   = 719;
	public static final int MONEY_ARR   = 791;
	public static final int MACADDR_ARR   = 1040;
	public static final int INET_ARR   = 1041;
	public static final int CIDR_ARR   = 651;
	public static final int ACLITEM_ARR   = 1034;
	public static final int BPCHAR_ARR   = 1014;
	public static final int VARCHAR_ARR   = 1015;
	public static final int DATE_ARR   = 1182;
	public static final int TIME_ARR   = 1183;
	public static final int TIMESTAMP_ARR   = 1115;
	public static final int TIMESTAMPTZ_ARR   = 1185;
	public static final int INTERVAL_ARR   = 1187;
	public static final int TIMETZ_ARR   = 1270;
	public static final int BIT_ARR   = 1561;
	public static final int VARBIT_ARR   = 1563;
	public static final int NUMERIC_ARR   = 1231;
	public static final int REFCURSOR_ARR   = 2201;
	public static final int REGPROCEDURE_ARR   = 2207;
	public static final int REGCLASS_ARR   = 2210;
	public static final int REGTYPE_ARR   = 2211;
	public static final int GPAOTID_ARR   = 3301;
	public static final int GPXLOGLOC_ARR   = 3311;

	/*
	 * length size
	 * */
	private static final int LONG_LEN = 8;
	private static final int BOOLEAN_LEN = 1;
	private static final int DOUBLE_LEN = 8;
	private static final int INT_LEN = 4;
	private static final int FLOAT_LEN = 4;
	private static final int SHORT_LEN = 2;

	/*
	 * Enum of the Database type
	 */
	public enum DBType {
		BIGINT(8, 8, false), BOOLEAN(1, 1, false), FLOAT8(8, 8, false), INTEGER(4, 4, false), REAL(4, 4, false),
		SMALLINT(2, 2, false), BYTEA(4, -1, false), TEXT(4, -1, true);

		private final int typelength; // -1 means var length
		private final int alignment;
		private final boolean isTextFormat;

		DBType(int align, int len, boolean isbin) {
			this.typelength = len;
			this.alignment  = align;
			this.isTextFormat = isbin;
		}

		public int getTypeLength() { return typelength; }
		public boolean isVarLength() { return typelength == -1; }

		// return the alignment requirement of the type
		public int getAlignment() { return alignment; }
	}

	/*
	 * Constants
	 */
	public static final int    DATA_VERSION = 1;
	public static final int    SCHEMA_VERSION = 2;
	private static final String CHARSET = "UTF-8";

	/*
	 * Local variables
	 */
	protected int[]    colType;
	protected Object[] colValue;

	/**
	 * An exception class for column type definition and
	 * set/get value mismatch.
	 * @author achoi
	 *
	 */
	public class TypeMismatchException extends IOException {
		public TypeMismatchException(String msg) {
			super(msg);
		}
	}

	/**
	 * Empty Constructor
	 */
	public GPDBWritable() {
	}

	/**
	 * Constructor to build a db record. colType defines the schema
	 * @param columnType the table column types
	 */
	public GPDBWritable(int[] columnType) {
		colType = columnType;
		colValue = new Object[columnType.length];
	}

	/**
	 * Constructor to build a db record from a serialized form.
	 * @param data a record in the serialized form
	 * @throws IOException if the data is malformatted.
	 */
	public GPDBWritable(byte[] data) throws IOException {
		ByteArrayInputStream bis = new ByteArrayInputStream(data);
		DataInputStream      dis = new DataInputStream(bis);

		this.readFields(dis);
	}

	/**
	 * return colType[]
	 */
	public int[] getColumnType(){
		return colType;
	}

	/**
	 * return colType[]
	 */
	public Object[] getColumnValue(){
		return colValue;
	}

	/**
	 *  Implement {@link org.apache.hadoop.io.Writable#readFields(DataInput)} 
	 */
	public void readFields(DataInput in) throws IOException {
		/* extract the version and col cnt */
		int pklen   = in.readInt();
		int version = in.readShort();

		/* !!! Check VERSION !!! */
		if (version != this.DATA_VERSION) {
			throw new IOException("Current GPDBWritable version(" +
					this.DATA_VERSION + ") does not match input version(" +
					version+")" + HDFSWriter.getLineData());
		}

		int colCnt  = in.readShort();
		int curOffset = 4+2+2;

		/* Extract Column Type */
		colType = new int[colCnt];
		DBType[] coldbtype = new DBType[colCnt];
		for(int i=0; i<colCnt; i++) {
			int enumType = (int)(in.readByte());
			curOffset += 1;
			if      (enumType == DBType.BIGINT.ordinal())   { colType[i] = BIGINT; coldbtype[i] = DBType.BIGINT;} 
			else if (enumType == DBType.BOOLEAN.ordinal())  { colType[i] = BOOLEAN; coldbtype[i] = DBType.BOOLEAN;}
			else if (enumType == DBType.FLOAT8.ordinal())   { colType[i] = FLOAT8;  coldbtype[i] = DBType.FLOAT8;}
			else if (enumType == DBType.INTEGER.ordinal())  { colType[i] = INTEGER; coldbtype[i] = DBType.INTEGER;}
			else if (enumType == DBType.REAL.ordinal())     { colType[i] = REAL; coldbtype[i] = DBType.REAL;}
			else if (enumType == DBType.SMALLINT.ordinal()) { colType[i] = SMALLINT; coldbtype[i] = DBType.SMALLINT;}
			else if (enumType == DBType.BYTEA.ordinal())    { colType[i] = BYTEA; coldbtype[i] = DBType.BYTEA;}
			else if (enumType == DBType.TEXT.ordinal())     { colType[i] = TEXT; coldbtype[i] = DBType.TEXT; }
			else throw new IOException("Unknown GPDBWritable.DBType ordinal value, DBType:" + enumType + HDFSWriter.getLineData());
		}

		/* Extract null bit array */
		byte[] nullbytes = new byte[getNullByteArraySize(colCnt)];
		in.readFully(nullbytes);
		curOffset += nullbytes.length;
		boolean[] colIsNull = byteArrayToBooleanArray(nullbytes, colCnt);

		/* extract column value */
		colValue = new Object[colCnt];
		for(int i=0; i<colCnt; i++) {
			if (!colIsNull[i]) {
				/* Skip the aligment padding */
				int skipbytes = roundUpAlignment(curOffset, coldbtype[i].getAlignment()) - curOffset;
				for(int j=0; j<skipbytes; j++)
					in.readByte();
				curOffset += skipbytes;

				/* For fixed length type, increment the offset according to type type length here.
				 * For var length type (BYTEA, TEXT), we'll read 4 byte length header and the
				 * actual payload.
				 */
				int varcollen = -1;
				if (coldbtype[i].isVarLength()) {
					varcollen = in.readInt();
					curOffset += 4 + varcollen;
				}
				else
					curOffset += coldbtype[i].getTypeLength();

				switch(colType[i]) {
					case BIGINT:   { long    val = in.readLong();    colValue[i] = new Long(val);    break; } 
					case BOOLEAN:  { boolean val = in.readBoolean(); colValue[i] = new Boolean(val); break; }
					case FLOAT8:   { double  val = in.readDouble();  colValue[i] = new Double(val);  break; }
					case INTEGER:  { int     val = in.readInt();     colValue[i] = new Integer(val); break; }
					case REAL:     { float   val = in.readFloat();   colValue[i] = new Float(val);   break; }
					case SMALLINT: { short   val = in.readShort();   colValue[i] = new Short(val);   break; }

					/* For BYTEA column, it has a 4 byte var length header. */
					case BYTEA:    { 
						colValue[i] = new byte[varcollen]; 
						in.readFully((byte[])colValue[i]);
						break;
					}
					/* For text formatted column, it has a 4 byte var length header
					 * and it's always null terminated string.
					 * So, we can remove the last "\0" when constructing the string.
					 */
					case TEXT:     {
						byte[] data = new byte[varcollen];
						in.readFully(data, 0, varcollen);
						colValue[i] = new String(data, 0, varcollen-1, CHARSET);
						break;
					}

					default:
						throw new IOException("Unknown GPDBWritable ColType, ColType:" + colType + HDFSWriter.getLineData());
				}
			}
		}

		/* Skip the ending aligment padding */
		int skipbytes = roundUpAlignment(curOffset, 8) - curOffset;
		for(int j=0; j<skipbytes; j++)
			in.readByte();
		curOffset += skipbytes;
	}

	/**
	 *  Implement {@link org.apache.hadoop.io.Writable#write(DataOutput)} 
	 */
	public void write(DataOutput out) throws IOException {
		int       numCol    = colType.length;
		boolean[] nullBits  = new boolean[numCol];
		int[]     colLength = new int[numCol];
		byte[]    enumType  = new byte[numCol];
		int[]     padLength = new int[numCol];
		byte[]    padbytes   = new byte[8];

		/**
		 * Compute the total payload and header length
		 * header = total length (4 byte), Version (2 byte), #col (2 byte)
		 * col type array = #col * 1 byte
		 * null bit array = ceil(#col/8)
		 */
		int datlen = 4+2+2;
		datlen += numCol;
		datlen += getNullByteArraySize(numCol);

		for(int i=0; i<numCol; i++) {
			/* Get the enum type */
			DBType coldbtype;
			switch(colType[i]) {
				case BIGINT:   coldbtype = DBType.BIGINT; break;
				case BOOLEAN:  coldbtype = DBType.BOOLEAN; break;
				case FLOAT8:   coldbtype = DBType.FLOAT8; break;
				case INTEGER:  coldbtype = DBType.INTEGER; break;
				case REAL:     coldbtype = DBType.REAL; break;
				case SMALLINT: coldbtype = DBType.SMALLINT; break;
				case BYTEA:    coldbtype = DBType.BYTEA; break;
				default:       coldbtype = DBType.TEXT; 
			}
			enumType[i] = (byte)(coldbtype.ordinal());

			/* Get the actual value, and set the null bit */
			if (colValue[i] == null) {
				nullBits[i]  = true;
				colLength[i] = 0;
			} else {
				nullBits[i]  = false;

				/* For binary format and fixed length type, we know it's fixed.
				 * 
				 * For binary format and var length type, the length is in the col value.
				 * 
				 * For text format, we need to add "\0" in the length.
				 */
				if (!isTextForm(colType[i]) && !coldbtype.isVarLength())
					colLength[i] = coldbtype.getTypeLength();
				else if (!isTextForm(colType[i]) && coldbtype.isVarLength())
					colLength[i] = ((byte[])colValue[i]).length;
				else
					colLength[i] = ((String)colValue[i]).getBytes(CHARSET).length + 1;

				/* Add type alignment padding.
				 * For variable length type, we added a 4 byte length header.
				 */
				padLength[i] = roundUpAlignment(datlen, coldbtype.getAlignment()) - datlen;
				datlen += padLength[i];
				if (coldbtype.isVarLength())
					datlen += 4;
			}
			datlen += colLength[i];
		}

		/*
		 * Add the final alignment padding for the next record
		 */
		int endpadding = roundUpAlignment(datlen, 8) - datlen;
		datlen += endpadding;

		/* the output data length, it should be eaqual to datlen, for safeguard*/
		int realLen = 0;

		/* Construct the packet header */
		out.writeInt(datlen);
		out.writeShort(DATA_VERSION);
		out.writeShort(numCol);
		realLen += INT_LEN + SHORT_LEN + SHORT_LEN;/*realLen += length of (datlen + DATA_VERSION + numCol)*/

		/* Write col type */
		for(int i=0; i<numCol; i++)
			out.writeByte(enumType[i]);
		realLen += numCol;

		/* Nullness */
		byte[] nullBytes = boolArrayToByteArray(nullBits);
		out.write(nullBytes);
		realLen += nullBytes.length;

		/* Column Value */
		for(int i=0; i<numCol; i++) {
			if (!nullBits[i]) {
				/* Pad the alignment byte first */
				if (padLength[i] > 0) {
					out.write(padbytes, 0, padLength[i]);
					realLen += padLength[i];
				}

				/* Now, write the actual column value */
				switch(colType[i]) {
					case BIGINT:   out.writeLong(   ((Long)   colValue[i]).longValue()); realLen += LONG_LEN;  break;
					case BOOLEAN:  out.writeBoolean(((Boolean)colValue[i]).booleanValue()); realLen += BOOLEAN_LEN; break;
					case FLOAT8:   out.writeDouble( ((Double) colValue[i]).doubleValue()); realLen += DOUBLE_LEN; break;
					case INTEGER:  out.writeInt(    ((Integer)colValue[i]).intValue()); realLen += INT_LEN; break;
					case REAL:     out.writeFloat(  ((Float)  colValue[i]).floatValue()); realLen += FLOAT_LEN; break;
					case SMALLINT: out.writeShort(  ((Short)  colValue[i]).shortValue()); realLen += SHORT_LEN; break;
					/* For BYTEA format, add 4byte length header at the beginning  */
					case BYTEA:
						out.writeInt(((byte[])colValue[i]).length);
						out.write((byte[])colValue[i]);
						realLen += INT_LEN + ((byte[])colValue[i]).length; /*realLen += length of (length + data)*/
						break;
					/* For text format, add 4byte length header (length include the "\0" at the end)
					 * at the beginning and add a "\0" at the end */
					default: {
						String outStr = (String)colValue[i]+"\0";
						byte[] data = (outStr).getBytes(CHARSET);
						out.writeInt(data.length);
						out.write(data);
						realLen += INT_LEN + data.length;/*realLen += length of (length + data)*/
						break;
					}
				}
			}
		}

		/* End padding */
		out.write(padbytes, 0, endpadding);
		realLen += endpadding;

		/* safeguard */
		if (datlen != realLen) {
			throw new IOException("data length error, data output size is not what expected");
		}
	}

	/**
	 * skip the table schema part, if we don't use it
	 * @throws IOException 
	 */
	public static void skipHead(DataInputStream dis) throws IOException {
		int schemaLen = dis.readInt();
		dis.read(new byte[schemaLen]);

//		dis.readInt(); //data total len, include append 0
	}

	/**
	 * Private helper to convert boolean array to byte array
	 */
	private static byte[] boolArrayToByteArray(boolean[] data) {
	    int len = data.length;
	    byte[] byts = new byte[getNullByteArraySize(len)];

	    for (int i = 0, j = 0, k = 7; i < data.length; i++) {
	        byts[j] |= (data[i] ? 1 : 0) << k--;
	        if (k < 0) { j++; k = 7; }
	    }
	    return byts;
	}

	/**
	 * Private helper to determine the size of the null byte array
	 */
	private static int getNullByteArraySize(int colCnt) {
		return (colCnt/8) + (colCnt%8 != 0 ? 1:0);
	}

	/**
	 * Private helper to convert byte array to boolean array
	 */
	private static boolean[] byteArrayToBooleanArray(byte[] data, int colCnt) {
	    boolean[] bools = new boolean[colCnt];
	    for (int i = 0, j = 0, k = 7; i < bools.length; i++) {
	        bools[i] = ((data[j] >> k--) & 0x01) == 1;
	        if (k < 0) { j++; k = 7; }
	    }
	    return bools;
	}

	/**
	 * Private helper to round up alignment for the given length
	 */
	private static int roundUpAlignment(int len, int align) {
		return (((len) + ((align) - 1)) & ~((align) - 1));
	}

	/**
	 * Return the byte form
	 * @return the binary representation of this object
	 * @throws if an I/O error occured
	 */
	public byte[] toBytes() throws IOException {
		ByteArrayOutputStream baos = new ByteArrayOutputStream(256);
		DataOutputStream      dos  = new DataOutputStream(baos);

		this.write(dos);

		return baos.toByteArray();
	}

	/**
	 * Getter/Setter methods to get/set the column value
	 */

	/**
	 * Set the column value of the record
	 * @param colIdx the column index
	 * @param val the value
	 * @throws TypeMismatchException the column type does not match
	 */
	public void setLong(int colIdx, Long val)
	throws TypeMismatchException {
		checkType(BIGINT, colIdx, true);
		colValue[colIdx] = val;
	}

	/**
	 * Set the column value of the record
	 * @param colIdx the column index
	 * @param val the value
	 * @throws TypeMismatchException the column type does not match
	 */
	public void setBoolean(int colIdx, Boolean val)
	throws TypeMismatchException {
		checkType(BOOLEAN, colIdx, true);
		colValue[colIdx] = val;
	}

	/**
	 * Set the column value of the record
	 * @param colIdx the column index
	 * @param val the value
	 * @throws TypeMismatchException the column type does not match
	 */
	public void setBytes(int colIdx, byte[] val)
	throws TypeMismatchException {
		checkType(BYTEA, colIdx, true);
		colValue[colIdx] = val;
	}

	/**
	 * Set the column value of the record
	 * @param colIdx the column index
	 * @param val the value
	 * @throws TypeMismatchException the column type does not match
	 */
	public void setString(int colIdx, String val)
	throws TypeMismatchException {
		checkType(TEXT, colIdx, true);
		colValue[colIdx] = val;
	}

	/**
	 * Set the column value of the record
	 * @param colIdx the column index
	 * @param val the value
	 * @throws TypeMismatchException the column type does not match
	 */
	public void setFloat(int colIdx, Float val)
	throws TypeMismatchException {
		checkType(REAL, colIdx, true);
		colValue[colIdx] = val;
	}

	/**
	 * Set the column value of the record
	 * @param colIdx the column index
	 * @param val the value
	 * @throws TypeMismatchException the column type does not match
	 */
	public void setDouble(int colIdx, Double val)
	throws TypeMismatchException {
		checkType(FLOAT8, colIdx, true);
		colValue[colIdx] = val;
	}

	/**
	 * Set the column value of the record
	 * @param colIdx the column index
	 * @param val the value
	 * @throws TypeMismatchException the column type does not match
	 */
	public void setInt(int colIdx, Integer val)
	throws TypeMismatchException {
		checkType(INTEGER, colIdx, true);
		colValue[colIdx] = val;
	}

	/**
	 * Set the column value of the record
	 * @param colIdx the column index
	 * @param val the value
	 * @throws TypeMismatchException the column type does not match
	 */
	public void setShort(int colIdx, Short val)
	throws TypeMismatchException {
		checkType(SMALLINT, colIdx, true);
		colValue[colIdx] = val;
	}

	/**
	 * Set the column value of the record
	 * @param colIdx the column index
	 * @throws TypeMismatchException the column type does not match
	 */
	public void setNull(int colIdx)
	throws TypeMismatchException {
		colValue[colIdx] = null;
	}

	/**
	 * Get the column value of the record
	 * @param colIdx the column index
	 * @throws TypeMismatchException the column type does not match
	 */
	public Long getLong(int colIdx)
	throws TypeMismatchException {
		checkType(BIGINT, colIdx, false);
		return (Long)colValue[colIdx];
	}

	/**
	 * Get the column value of the record
	 * @param colIdx the column index
	 * @throws TypeMismatchException the column type does not match
	 */
	public Boolean getBoolean(int colIdx)
	throws TypeMismatchException {
		checkType(BOOLEAN, colIdx, false);
		return (Boolean)colValue[colIdx];
	}

	/**
	 * Get the column value of the record
	 * @param colIdx the column index
	 * @throws TypeMismatchException the column type does not match
	 */
	public byte[] getBytes(int colIdx)
	throws TypeMismatchException {
		checkType(BYTEA, colIdx, false);
		return (byte[])colValue[colIdx];
	}

	/**
	 * Get the column value of the record
	 * @param colIdx the column index
	 * @throws TypeMismatchException the column type does not match
	 */
	public String getString(int colIdx)
	throws TypeMismatchException {
		checkType(TEXT, colIdx, false);
		return (String)colValue[colIdx];
	}

	/**
	 * Get the column value of the record
	 * @param colIdx the column index
	 * @throws TypeMismatchException the column type does not match
	 */
	public Float getFloat(int colIdx)
	throws TypeMismatchException {
		checkType(REAL, colIdx, false);
		return (Float)colValue[colIdx];
	}

	/**
	 * Get the column value of the record
	 * @param colIdx the column index
	 * @throws TypeMismatchException the column type does not match
	 */
	public Double getDouble(int colIdx)
	throws TypeMismatchException {
		checkType(FLOAT8, colIdx, false);
		return (Double)colValue[colIdx];
	}

	/**
	 * Get the column value of the record
	 * @param colIdx the column index
	 * @throws TypeMismatchException the column type does not match
	 */
	public Integer getInt(int colIdx)
	throws TypeMismatchException {
		checkType(INTEGER, colIdx, false);
		return (Integer)colValue[colIdx];
	}

	/**
	 * Get the column value of the record
	 * @param colIdx the column index
	 * @throws TypeMismatchException the column type does not match
	 */
	public Short getShort(int colIdx)
	throws TypeMismatchException {
		checkType(SMALLINT, colIdx, false);
		return (Short)colValue[colIdx];
	}

	/**
	 * Return a string representation of the object
	 */
	public String toString() {
		if (colType == null)
			return null;

		String result= "";
		for(int i=0; i<colType.length; i++) {
			result += "Column " + i + ":";
			if (colValue[i] != null) {
				if (colType[i] == BYTEA)
					result += byteArrayInString((byte[])colValue[i]);
				else
					result += colValue[i].toString();
			}
			result += "\n";
		}
		return result;
	}

	/**
	 * Helper printing function
	 */
	private static String byteArrayInString(byte[] data) {
		String result = "";
		for(int i=0; i<data.length; i++) {
			Byte b = new Byte(data[i]);
			result += b.intValue() + " ";
		}
		return result;
	}


	/**
	 * Private Helper to check the type mismatch
	 * If the expected type is stored as string, then it must be set
	 * via setString.
	 * Otherwise, the type must match.
	 */
	private void checkType(int inTyp, int idx, boolean isSet)
	throws TypeMismatchException {
		if (idx < 0 || idx >= colType.length)
			throw new TypeMismatchException(
					"Column index is out of range");

		int exTyp = colType[idx];
		String errmsg;

		if (isTextForm(exTyp)) {
			if (inTyp != TEXT)
				throw new TypeMismatchException(formErrorMsg(inTyp, TEXT, isSet));
		} else if (inTyp != exTyp)
			throw new TypeMismatchException(formErrorMsg(inTyp, exTyp, isSet));
	}

	private String formErrorMsg(int inTyp, int colTyp, boolean isSet) {
		String errmsg;
		if (isSet)
			errmsg = "Cannot set "+getTypeName(inTyp) + " to a " +
				getTypeName(colTyp) + " column";
		else
			errmsg = "Cannot get "+getTypeName(inTyp) + " from a " +
				getTypeName(colTyp) + " column";
		return errmsg;
	}

	/**
	 * Private Helper routine to tell whether a type is Text form or not
	 * @param type the type OID that we want to check
	 */
	private boolean isTextForm(int type) {
		if (type==BIGINT ||
			type==BOOLEAN ||
			type==BYTEA ||
			type==FLOAT8 ||
			type==INTEGER ||
			type==REAL ||
			type==SMALLINT)
			return false;
		return true;
	}

	/**
	 * Private Helper to get the type name for rasing error.
	 * If a given oid is not in the commonly used list, we
	 * would expect a TEXT for it (for the error message).
	 */
	private static String getTypeName(int oid) {
		switch(oid) {
		case BOOLEAN  : return "BOOLEAN";
		case BYTEA    : return "BYTEA";
		case CHAR     : return "CHAR";
		case BIGINT   : return "BIGINT";
		case SMALLINT : return "SMALLINT";
		case INTEGER  : return "INTEGER";
		case TEXT     : return "TEXT";
		case REAL     : return "REAL";
		case FLOAT8   : return "FLOAT8";
		case BPCHAR   : return "BPCHAR";
		case VARCHAR  : return "VARCHAR";
		case DATE     : return "DATE";
		case TIME     : return "TIME";
		case TIMESTAMP: return "TIMESTAMP";
		case NUMERIC  : return "NUMERIC";
		default:        return "TEXT";
		}
	}

	@Deprecated
	public static boolean isArrayType(int type) {
		if (type == GPXLOGLOC_ARR || type == GPAOTID_ARR || type == REGTYPE_ARR || type == REGCLASS_ARR
				|| type == REGPROCEDURE_ARR || type == REFCURSOR_ARR || type == NUMERIC_ARR || type == VARBIT_ARR
				|| type == BIT_ARR || type == TIMETZ_ARR || type == INTERVAL_ARR || type == TIMESTAMP_ARR
				|| type == TIME_ARR || type == DATE_ARR || type == VARCHAR_ARR || type == BPCHAR_ARR
				|| type == ACLITEM_ARR || type == CIDR_ARR || type == INET_ARR || type == MACADDR_ARR
				|| type == MONEY_ARR || type == CIRCLE_ARR || type == TINTERVAL_ARR || type == RELTIME_ARR
				|| type == ABSTIME_ARR || type == FLOAT8_ARR || type == FLOAT4_ARR || type == LINE_ARR
				|| type == POLYGON_ARR || type == BOX_ARR || type == PATH_ARR || type == LSSEG_ARR
				|| type == POINT_ARR || type == XML_ARR || type == OIDVECTOR_ARR || type == CID_ARR
				|| type == XID_ARR || type == TID_ARR || type == OID_ARR || type == TEXT_ARR
				|| type == REGPROC_ARR || type == INT4_ARR || type == INT2VECTOR_ARR || type == INT2_ARR
				|| type == INT8_ARR || type == NAME_ARR || type == CHAR_ARR || type == BYTEA_ARR || type == BOOLEAN_ARR) {
			return true;
		}
		return false;
	}

	public static int getElementTypeFromArrayType(int type) {
		switch (type) {
		case FLOAT8_ARR:
			return FLOAT8;
		case FLOAT4_ARR:
			return REAL;
		case INT4_ARR:
		case INT2_ARR:
			return INTEGER;
		case INT8_ARR:
			return BIGINT;
		case BYTEA_ARR:
			return BYTEA;
		case BOOLEAN_ARR:
			return BOOLEAN;
		case TEXT_ARR:
		default:
			return TEXT;
		}
	}

	public static boolean isNumberType(int type) {
		return ( type == INTEGER || type == BIGINT || type == REAL || type == FLOAT8 || type == SMALLINT 
				|| type == INT4_ARR || type == INT8_ARR || type == FLOAT4_ARR || type == FLOAT8_ARR || type == INT2_ARR);
	}

	public static boolean isNumericOrNumericArray(int type) {
		return type == NUMERIC || type == NUMERIC_ARR;
	}

	public static boolean isBigintOrBigintArray(int type) {
		return type == BIGINT || type == INT8_ARR;
	}

	public static boolean isRealOrRealArray(int type) {
		return type == REAL || type == FLOAT4_ARR;
	}

	public static boolean isDoubleOrDoubleArray(int type) {
		return type == FLOAT8 || type == FLOAT8_ARR;
	}

	private static boolean isSmallIntOrSmallIntArray(int type) {
		return type == SMALLINT || type == INT2_ARR;
	}

	public static boolean isIntOrIntArray(int type) {
		return type == INTEGER || type == INT4_ARR;
	}

	public static boolean isTextOrTextArray(int type) {
		return type == TEXT || type == TEXT_ARR;
	}

	public static boolean isByteaOrByteaArray(int type) {
		return type == BYTEA || type == BYTEA_ARR;
	}

	public static boolean isBoolOrBoolArray(int type) {
		return type == BOOLEAN || type == BOOLEAN_ARR;
	}

	public static boolean isRealOrDouble(int type) {
		return isRealOrRealArray(type) || isDoubleOrDoubleArray(type);
	}

	public static boolean isIntOrBigInt(int type) {
		return isIntOrIntArray(type) || isBigintOrBigintArray(type);
	}

	public static boolean isSmallIntOrIntOrBigInt(int type) {
		return isIntOrIntArray(type) || isBigintOrBigintArray(type) || isSmallIntOrSmallIntArray(type);
	}

	public static boolean isArrayType(int colType2, int ndims) {
		return ndims > 0;
	}

	public static boolean isDateOrDateArray(int type) {
		return type == DATE || type == DATE_ARR;
	}

	public static boolean isTimeOrTimeArray(int type) {
		return type == TIME || type == TIME_ARR;
	}

	public static boolean isTimestampOrTimestampArray(int type) {
		return type == TIMESTAMP || type == TIMESTAMP_ARR;
	}

	public static boolean isIntervalOrIntervalArray(int type) {
		return type == INTERVAL || type == INTERVAL_ARR;
	}

	public static boolean isXmlOrXmlArray(int type) {
		return type == XML || type == XML_ARR;
	}

	/**
	 * check whether the value of a column is null
	 */
	public boolean isNull(int index) {
		return colValue[index] == null;
	}

}
