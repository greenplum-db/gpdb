// Required by building CPP dynamic library via Makefile of GPDB.
#define PGDLLIMPORT "C"

#include <signal.h>
#include <cstdlib>

#include <openssl/err.h>
#include <pthread.h>

#include "postgres.h"

#include "access/extprotocol.h"
#include "catalog/pg_proc.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

#include "gpreader.h"
#include "s3common.h"
#include "s3conf.h"
#include "s3log.h"
#include "s3utils.h"

/* Do the module magic dance */
PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(s3_export);
PG_FUNCTION_INFO_V1(s3_import);

extern "C" {
Datum s3_export(PG_FUNCTION_ARGS);
Datum s3_import(PG_FUNCTION_ARGS);
}

void check_essential_config() {
    if (s3ext_accessid == "") {
        ereport(ERROR, (0, errmsg("ERROR: access id is empty")));
    }

    if (s3ext_secret == "") {
        ereport(ERROR, (0, errmsg("ERROR: secret is empty")));
    }

    if ((s3ext_segnum == -1) || (s3ext_segid == -1)) {
        ereport(ERROR, (0, errmsg("ERROR: segment id is invalid")));
    }
}

/*
 * Import data into GPDB.
 * invoked by GPDB, be careful with C++ exceptions.
 */
Datum s3_import(PG_FUNCTION_ARGS) {
    /* Must be called via the external table format manager */
    if (!CALLED_AS_EXTPROTOCOL(fcinfo))
        elog(ERROR, "extprotocol_import: not called by external protocol manager");

    /* Get our internal description of the protocol */
    GPReader *gpreader = (GPReader *)EXTPROTOCOL_GET_USER_CTX(fcinfo);

    /* last call. destroy reader */
    if (EXTPROTOCOL_IS_LAST_CALL(fcinfo)) {
        thread_cleanup();

        if (!reader_cleanup(&gpreader)) {
            ereport(ERROR, (0, errmsg("Failed to cleanup S3 extension: %s",
                                      gpReaderErrorMessage.c_str())));
        }

        EXTPROTOCOL_SET_USER_CTX(fcinfo, NULL);

        PG_RETURN_INT32(0);
    }

    /* first call. do any desired init */
    if (gpreader == NULL) {
        const char *url_with_options = EXTPROTOCOL_GET_URL(fcinfo);

        thread_setup();

        gpreader = reader_init(url_with_options);
        if (!gpreader) {
            ereport(ERROR, (0, errmsg("Failed to init S3 extension, segid = %d, "
                                      "segnum = %d, please check your "
                                      "configurations and net connection: %s",
                                      s3ext_segid, s3ext_segnum, gpReaderErrorMessage.c_str())));
        }

        check_essential_config();

        EXTPROTOCOL_SET_USER_CTX(fcinfo, gpreader);
    }

    char *data_buf = EXTPROTOCOL_GET_DATABUF(fcinfo);
    int32 data_len = EXTPROTOCOL_GET_DATALEN(fcinfo);

    if (!reader_transfer_data(gpreader, data_buf, data_len)) {
        ereport(ERROR,
                (0, errmsg("s3_import: could not read data: %s", gpReaderErrorMessage.c_str())));
    }
    PG_RETURN_INT32(data_len);
}

/*
 * Export data out of GPDB.
 * invoked by GPDB, be careful with C++ exceptions.
 */
Datum s3_export(PG_FUNCTION_ARGS) {
    PG_RETURN_INT32(0);
}
