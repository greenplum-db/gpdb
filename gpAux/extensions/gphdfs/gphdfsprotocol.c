/*-------------------------------------------------------------------------
 *
 * gphdfsprotocol.c
 *
 * This protocol starts a java program HDFSReader/Writer which does the
 * actual "protocol" work. This protocol is just like the external table
 * execute framework: starts the external program and pipe via standard
 * in/out.
 *
 * Copyright (c) 2011, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <fcntl.h>
#include <unistd.h>

#include "fmgr.h"
#include "funcapi.h"
#include "access/extprotocol.h"
#include "catalog/pg_proc.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "access/fileam.h"
#include "catalog/pg_exttable.h"
#include "utils/guc.h"
#include "miscadmin.h"

#include "utils/lsyscache.h"

/* Do the module magic dance */
PG_FUNCTION_INFO_V1(gphdfsprotocol_export);
PG_FUNCTION_INFO_V1(gphdfsprotocol_import);
PG_FUNCTION_INFO_V1(gphdfsprotocol_validate_urls);

Datum gphdfsprotocol_export(PG_FUNCTION_ARGS);
Datum gphdfsprotocol_import(PG_FUNCTION_ARGS);
Datum gphdfsprotocol_validate_urls(PG_FUNCTION_ARGS);

extern void appendIntToBuffer(StringInfo buf, int val);
extern void appendInt1ToBuffer(StringInfo buf, uint8 val);
extern void appendInt2ToBuffer(StringInfo buf, uint16 val);

typedef struct {
	URL_FILE *importFile;
	bool      importDone;
} gphdfs_import_t;

typedef struct {
	const char *targetHadoopVersion;
	const char *connectorVersion;
} hadoop_vers_to_connector_ver;

/**
 * Mapping from "target hadoop version" (gp_hadoop_target_version GUC)
 * to the connecto version (actually, this is also the jar file without the .jar).
 */
static hadoop_vers_to_connector_ver hdVer_to_connVer[] =
{
	{"hadoop2",  "hadoop2-gnet-1.2.0.0"},
	{"hdp2.0",   "hdp2.0-gnet-1.2.0.0"},
	{"cdh5.0",   "cdh5.0-gnet-1.2.0.0"},
	{"mpr4.0",   "mpr4.0-gnet-1.2.0.0"},
	/* End-of-list marker */
	{NULL, NULL}
};

static const char*
getConnectorVersion()
{
	int i=0;
	Insist(gp_hadoop_target_version);
	for(; hdVer_to_connVer[i].connectorVersion; i++)
	{
		if ((strlen(gp_hadoop_target_version) ==
			 strlen(hdVer_to_connVer[i].targetHadoopVersion)) &&
			(strncmp(gp_hadoop_target_version,
					 hdVer_to_connVer[i].targetHadoopVersion,
					 strlen(gp_hadoop_target_version)) == 0))
			return hdVer_to_connVer[i].connectorVersion;
	}

	ereport(ERROR,
			(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
			 errmsg("target Hadoop version \"%s\" is not supported", gp_hadoop_target_version),
			 errhint("please use one of 'hadoop2', 'hdp2.0', 'mpr4.0', 'cdh5.0'")));

	return "N/A";
}

/*
 * Using the Hadoop connector requires proper setup of GUCs
 * This procedure does sanity check on gp_hadoop_connector_jardir,
 * gp_hadoop_target_version and gp_hadoop_home.
 * It also update GUC gp_hadoop_connector_version for the current gp_hadoop_target_version.
 *
 * It checks the following:
 * 1. $GPHOME/<gp_hadoop_jardir>/$GP_HADOOP_CONN_VERSION.jar must exists.
 * 2. if gp_hadoop_home is set, then gp_hadoop_home must exists.
 */
static void
checkHadoopGUCs()
{
	char gphome[MAXPGPATH];
	StringInfoData path;
	int  jarFD;

	/* Check the existence of $GPHOME/<gp_hadoop_jardir>/$GP_HADOOP_CONN_VERSION.jar
	 *
	 * To get $GPHOME, we go from my_exec_path, which is $GPHOME/bin/postgres, and
	 * go up 2 levels.
	 *
	 * Currently, gp_hadoop_connector_jardir is fixed. We look up $GP_HADOOP_CONN_VERSION
	 * using gp_hadoop_target_version.
	 */
	snprintf(gphome, sizeof(gphome), "%s", my_exec_path);
	get_parent_directory(gphome);
	get_parent_directory(gphome);

	initStringInfoOfSize(&path, MAXPGPATH);
	gp_hadoop_connector_version = (char*)getConnectorVersion();
	appendStringInfo(&path, "%s/%s/%s.jar",
			gphome, gp_hadoop_connector_jardir, gp_hadoop_connector_version);

	jarFD = BasicOpenFile(path.data, O_RDONLY | PG_BINARY, 0);
	if (jarFD == -1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("cannot open Hadoop Cross Connect in %s: %m", path.data)));
	}
	close(jarFD);


	/* Check the existence of gp_hadoop_home, if specified.
	 *
	 * If user has already specified $HADOOP_HOME in the env, then
	 * there's no need to setup this GUC.
	 */
	if (strlen(gp_hadoop_home)> 0)
	{
		int hdHomeFD = BasicOpenFile(gp_hadoop_home, O_RDONLY, 0);
		if (hdHomeFD == -1)
		{
			ereport(ERROR,
					(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
					 errmsg("cannot open gp_hadoop_home in %s: %m", gp_hadoop_home)));
		}
		close(hdHomeFD);
	}
}

/**
 * Strong quoting for command line argument
 *
 * some input arguements, such as URI, are not validated.
 * we need to strong quote it.
 *
 * Input:
 *   value - null terminated string
 *
 * Return:
 *   a strong quoted null terminated string
 */
static char*
quoteArgument(char* value)
{
	StringInfoData quotedVal;
	char *valptr;

	/* Guess the size of the quoted one
	 * I don't think it's common to have quote inside the URI.
	 * So, let's guess we only need to account for the begin/end quote and 3 more.
	 * That means 5 more char than the input value.
	 */
	initStringInfoOfSize(&quotedVal, strlen(value)+5);

	/* It starts with a quote. */
	appendStringInfoChar(&quotedVal, '\'');

	/* Copy each char and append to quotedVal
	 * if the char is a quote or a slash, escape it.
	 */
	for(valptr=value; *value != 0; value++)
	{
		char chr = *value;
		if (chr == '\'' || chr == '\\')
			appendStringInfoChar(&quotedVal, '\\');
		appendStringInfoChar(&quotedVal, chr);
	}

	/* It ends with a quote. */
	appendStringInfoChar(&quotedVal, '\'');

	return quotedVal.data;
}

static bool hasIllegalCharacters(char *str)
{
	if (str == NULL)
	{
		return false;
	}

	for (; *str; str++)
	{
		if (*str == '\\' || *str == '\'' || *str == '<' || *str == '>')
		{
			return true;
		}
	}

	return false;
}

/**
 * Open/Init of the gphdfs protocol
 *
 * It setup the Hadoop env var by calling hadoop_env.sh.
 * Then it calls the corresponding java program to do the actual
 * read/write.
 */
static URL_FILE
*gphdfs_fopen(PG_FUNCTION_ARGS, bool forwrite)
{
	URL_FILE      *myData;
	StringInfoData cmd;
	StringInfoData env_cmd;
	StringInfoData table_schema;
	StringInfoData table_attr_names;
	char          *java_cmd;
	extvar_t       extvar;
	char          *url;
	Relation       rel;
	ExtTableEntry *exttbl;
	char          *format;

	/* Before we start, make sure that all the GUCs are set properly.
	 * This will also set the gp_hadoop_connector_version global var.
	 */
	checkHadoopGUCs();

	/* The env setup script */
	initStringInfo(&env_cmd);
	appendStringInfo(&env_cmd, "source $GPHOME/%s/hadoop_env.sh;", gp_hadoop_connector_jardir);

	/* The java program. See the java program for details */
	if (forwrite)
		java_cmd = "java $GP_JAVA_OPT -classpath $CLASSPATH com.emc.greenplum.gpdb.hdfsconnector.HDFSWriter $GP_SEGMENT_ID $GP_XID\0";
	else
		java_cmd = "java $GP_JAVA_OPT -classpath $CLASSPATH com.emc.greenplum.gpdb.hdfsconnector.HDFSReader $GP_SEGMENT_ID $GP_SEGMENT_COUNT\0";

	/* NOTE: I've to assume that if it's not TEXT, it's going to be the RIGHT
	 * custom format. There's no easy way to find out the name of the formatter here.
	 * If the wrong formatter is used, we'll see some error in the protocol.
	 * No big deal.
	 */
	rel    = EXTPROTOCOL_GET_RELATION(fcinfo);
	exttbl = GetExtTableEntry(rel->rd_id);
	format = (fmttype_is_text(exttbl->fmtcode) || fmttype_is_csv(exttbl->fmtcode)) ? "TEXT":"GPDBWritable";
	if (fmttype_is_avro(exttbl->fmtcode))
	{
		format = "AVRO";
	} else if (fmttype_is_parquet(exttbl->fmtcode))
	{
		format = "PARQUET";
	}

	/* we transfer table's schema info together with its url */
	if (!forwrite)
	{
		initStringInfo(&table_schema);
		initStringInfo(&table_attr_names);

		int colnum = rel->rd_att->natts;
		for (int i =0; i < colnum; i++)
		{
			int typid = rel->rd_att->attrs[i]->atttypid;

			/* add type delimiter, for udt, it can be anychar */
			char delim = 0;
			int16 typlen;
			bool typbyval;
			char typalien;
			Oid typioparam;
			Oid func;
			get_type_io_data(typid, IOFunc_input, &typlen, &typbyval, &typalien, &delim, &typioparam, &func);

			char out[20] = {0};
			sprintf(out, "%010d%d%d%03d", typid, rel->rd_att->attrs[i]->attnotnull,
				rel->rd_att->attrs[i]->attndims, delim);

			appendStringInfoString(&table_schema, out);

			char name[70] = {0};
			sprintf(name, "%s%c", rel->rd_att->attrs[i]->attname.data, ',');
			appendStringInfoString(&table_attr_names, name);
		}
	}
	/* Form the actual command
	 *
	 * 1. calls the env setup script
	 * 2. append the remaining arguements: <format>, <conn ver> and <url> to the java command
	 *
	 * Note: "url" has to be quoted because it's an unverified user input
	 * Note: gp_hadoop_connector_version does not need to be quoted
	 *       because we've verified it in checkHadoopGUCs().
	 */

	/* Note: if url is passed with E prefix, quote simply quote has no effect,
	 * we filter some dangerous chararacters right now. */
	char* url_user = EXTPROTOCOL_GET_URL(fcinfo);
	if (hasIllegalCharacters(url_user))
	{
		ereport(ERROR, (0, errmsg("illegal char in url")));
	}

	url = quoteArgument(EXTPROTOCOL_GET_URL(fcinfo));
	initStringInfo(&cmd);

	appendStringInfo(&cmd, EXEC_URL_PREFIX "%s%s %s %s %s", env_cmd.data, java_cmd, format,
			gp_hadoop_connector_version, url);

	if (!forwrite)
	{
		appendStringInfo(&cmd, " '%s'", table_schema.data);
		pfree(table_schema.data);

		appendStringInfo(&cmd, " '%s'", table_attr_names.data);
		pfree(table_attr_names.data);
	}

	/* Setup the env and run the script..
	 *
	 * NOTE: the last argument to external_set_env_vars is set to ZERO because we
	 * don't have access to the scan counter at all. It's ok because we don't need it.
	 */
	external_set_env_vars(&extvar, url, false, NULL, NULL, false, 0);
	myData = url_execute_fopen(cmd.data, forwrite, &extvar, NULL);

	/* Free the command string */
	pfree(cmd.data);

	return myData;
}

/*
 * Import data into GPDB.
 */
Datum
gphdfsprotocol_import(PG_FUNCTION_ARGS)
{
	char     *data;
	int	      datlen;
	size_t    nread = 0;
	gphdfs_import_t *myData;

	/* Must be called via the external table format manager */
	if (!CALLED_AS_EXTPROTOCOL(fcinfo))
		elog(ERROR, "cannot execute gphdfsprotocol_import outside protocol manager");

	/* Get our internal description of the protocol */
	myData = (gphdfs_import_t*) EXTPROTOCOL_GET_USER_CTX(fcinfo);

	/* =======================================================================
	 *                            DO LAST CALL
	 *                            Nothing to be done if it has already been closed
	 * ======================================================================= */
	if (EXTPROTOCOL_IS_LAST_CALL(fcinfo))
	{
		if (myData != NULL && !myData->importDone)
			url_fclose(myData->importFile, false, "gphdfs protocol");
		PG_RETURN_INT32(0);
	}

	/* =======================================================================
	 *                            DO OPEN
	 * ======================================================================= */
	if (myData == NULL)
	{
		myData = palloc(sizeof(gphdfs_import_t));
		myData->importFile = gphdfs_fopen(fcinfo, false);
		myData->importDone = false;
		EXTPROTOCOL_SET_USER_CTX(fcinfo, myData);
	}

	/* =======================================================================
	 *                            DO THE IMPORT
	 * ======================================================================= */
	data 	= EXTPROTOCOL_GET_DATABUF(fcinfo);
	datlen 	= EXTPROTOCOL_GET_DATALEN(fcinfo);

	if (datlen > 0 && !myData->importDone)
		nread = url_execute_fread(data, datlen, myData->importFile, NULL);

	/* =======================================================================
	 *                            DO CLOSE
	 *                            close early to raise error early
	 * ======================================================================= */
	if (nread == 0)
	{
		myData->importDone = true;
		url_fclose(myData->importFile, true, "gphdfs protocol");
	}

	PG_RETURN_INT32((int)nread);
}

/*
 * Export data out of GPDB.
 */
Datum
gphdfsprotocol_export(PG_FUNCTION_ARGS)
{
	URL_FILE *myData;
	char     *data;
	int       datlen;
	size_t    wrote = 0;
	static char	ebuf[512] = {0};
	int	    	ebuflen = 512;

	/* Must be called via the external table format manager */
	if (!CALLED_AS_EXTPROTOCOL(fcinfo))
		elog(ERROR, "cannot execute gphdfsprotocol_export outside protocol manager");

	/* Get our internal description of the protocol */
	myData = (URL_FILE *) EXTPROTOCOL_GET_USER_CTX(fcinfo);

	/* =======================================================================
	 *                            DO CLOSE
	 * ======================================================================= */
	if (EXTPROTOCOL_IS_LAST_CALL(fcinfo))
	{
		if (myData)
			url_fclose(myData, true, "gphdfs protocol");
		PG_RETURN_INT32(0);
	}

	/* =======================================================================
	 *                            DO OPEN
	 * ======================================================================= */
	if (myData == NULL)
	{
		myData = gphdfs_fopen(fcinfo, true);
		EXTPROTOCOL_SET_USER_CTX(fcinfo, myData);

		/* add schema info to pipe */
		StringInfo schema_data = makeStringInfo();

		Relation relation = FORMATTER_GET_RELATION(fcinfo);
		ExtTableEntry *exttbl = GetExtTableEntry(relation->rd_id);
		if (fmttype_is_avro(exttbl->fmtcode) || fmttype_is_parquet(exttbl->fmtcode) )
		{
			int relNameLen = strlen(relation->rd_rel->relname.data);
			appendIntToBuffer(schema_data, relNameLen);
			appendBinaryStringInfo(schema_data, relation->rd_rel->relname.data, relNameLen);

			int ncolumns = relation->rd_att->natts;
			appendIntToBuffer(schema_data, ncolumns);
			int i = 0;
			for (; i< ncolumns; i++)
			{
				Oid type = relation->rd_att->attrs[i]->atttypid;

				/* add attname,atttypid,attnotnull,attndims to schema_data filed */
				int attNameLen = strlen(relation->rd_att->attrs[i]->attname.data);
				appendIntToBuffer(schema_data, attNameLen);
				appendBinaryStringInfo(schema_data, relation->rd_att->attrs[i]->attname.data, attNameLen);

				appendIntToBuffer(schema_data, type);

				bool notNull = relation->rd_att->attrs[i]->attnotnull;
				appendInt1ToBuffer(schema_data, notNull?1:0);

				appendIntToBuffer(schema_data, relation->rd_att->attrs[i]->attndims);

				/* add type delimiter, for udt, it can be anychar */
				char delim = 0;
				int16 typlen;
				bool typbyval;
				char typalien;
				Oid typioparam;
				Oid func;
				get_type_io_data(type, IOFunc_input, &typlen, &typbyval, &typalien, &delim, &typioparam, &func);
				appendInt1ToBuffer(schema_data, delim);
			}

			StringInfo schema_head = makeStringInfo();
			appendIntToBuffer(schema_head, schema_data->len + 2);
			appendInt2ToBuffer(schema_head, 2);

			url_execute_fwrite(schema_head->data, schema_head->len, myData, NULL);
			url_execute_fwrite(schema_data->data, schema_data->len, myData, NULL);

			pfree(schema_head->data);
			pfree(schema_data->data);
		}
	}


	/* =======================================================================
	 *                            DO THE EXPORT
	 * ======================================================================= */
	data   = EXTPROTOCOL_GET_DATABUF(fcinfo);
	datlen = EXTPROTOCOL_GET_DATALEN(fcinfo);

	if (datlen > 0)
		wrote = url_execute_fwrite(data, datlen, myData, NULL);

	if (url_ferror(myData, wrote, ebuf, ebuflen))
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 strlen(ebuf) > 0 ? errmsg("could not write to external resource:\n%s",ebuf) :
				 errmsg("could not write to external resource: %m")));
	}

	PG_RETURN_INT32((int)wrote);
}

/**
 * Validate the URLs
 */
Datum
gphdfsprotocol_validate_urls(PG_FUNCTION_ARGS)
{
	/* Must be called via the external table format manager */
	if (!CALLED_AS_EXTPROTOCOL_VALIDATOR(fcinfo))
		elog(ERROR, "cannot execute gphdfsprotocol_validate_urls outside protocol manager");

	/*
	 * Condition 1: there must be only ONE url.
	 */
	if (EXTPROTOCOL_VALIDATOR_GET_NUM_URLS(fcinfo) != 1)
            ereport(ERROR,
                    (errcode(ERRCODE_PROTOCOL_VIOLATION),
                     errmsg("number of URLs must be one")));

	/* Check for illegal characters. */
	char* url_user = EXTPROTOCOL_VALIDATOR_GET_NTH_URL(fcinfo, 1);
	if (hasIllegalCharacters(url_user))
	{
		ereport(ERROR, (0, errmsg("illegal char in url")));
	}

	PG_RETURN_VOID();
}
