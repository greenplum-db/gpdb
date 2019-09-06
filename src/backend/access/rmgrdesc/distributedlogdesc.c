/*-------------------------------------------------------------------------
 *
 * distributedlogdesc.c
 *	  rmgr descriptor routines for access/transam/distributedlog.c
 *
 * Portions Copyright (c) 1996-2013, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/distributedlogdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/distributedlog.h"


void
DistributedLog_desc(StringInfo buf, XLogReaderState *record)
{
	uint8		info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
	char		*rec = XLogRecGetData(record);

	if (info == DISTRIBUTEDLOG_ZEROPAGE)
	{
		int			page;

		memcpy(&page, rec, sizeof(int));
		appendStringInfo(buf, "zeropage: %d", page);
	}
	else if (info == DISTRIBUTEDLOG_TRUNCATE)
	{
		int			page;

		memcpy(&page, rec, sizeof(int));
		appendStringInfo(buf, "truncate before: %d", page);
	}
	else
		appendStringInfo(buf, "UNKNOWN");
}

const char *
DistributedLog_identify(uint8 info)
{
	const char *id = NULL;

	switch (info & ~XLR_INFO_MASK)
	{
		case DISTRIBUTEDLOG_ZEROPAGE:
			id = "DISTRIBUTEDLOG_ZEROPAGE";
			break;
		case DISTRIBUTEDLOG_TRUNCATE:
			id = "DISTRIBUTEDLOG_TRUNCATE";
			break;
	}

	return id;
}
