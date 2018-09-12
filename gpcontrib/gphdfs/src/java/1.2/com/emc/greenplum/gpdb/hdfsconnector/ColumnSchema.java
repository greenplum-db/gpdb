package com.emc.greenplum.gpdb.hdfsconnector;

public class ColumnSchema {
	String columName;
	int typeId;
	int ndims;
	char delim;
	int notnull;

	public ColumnSchema(int typeId, int notNull, int ndims, char delim) {
		this.typeId = typeId;
		this.notnull = notNull;
		this.ndims = ndims;
		this.delim = delim;
	}

	public ColumnSchema(String colName, int typeId, int notNull, int ndims, char delim) {
		this.columName = colName;
		this.typeId = typeId;
		this.notnull = notNull;
		this.ndims = ndims;
		this.delim = delim;
	}

	public String getColumName() {
		return columName;
	}

	public void setColumName(String columName) {
		this.columName = columName;
	}

	public int getType() {
		return typeId;
	}
	public void setType(int type) {
		this.typeId = type;
	}
	public int getNdim() {
		return ndims;
	}
	public void setNdim(int ndim) {
		this.ndims = ndim;
	}
	public char getDelim() {
		return delim;
	}
	public void setDelim(char delim) {
		this.delim = delim;
	}
	public int getIsnull() {
		return notnull;
	}
	public void setIsnull(int isnull) {
		this.notnull = isnull;
	}


}
