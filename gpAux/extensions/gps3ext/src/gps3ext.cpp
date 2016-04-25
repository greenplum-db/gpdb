#define PGDLLIMPORT "C"

#include <cstdlib>

#include <pthread.h>
#include <signal.h>

#include "postgres.h"

#include "funcapi.h"

#include "access/extprotocol.h"
#include "catalog/pg_proc.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

#include <openssl/err.h>

#include "gps3ext.h"
#include "s3common.h"
#include "s3conf.h"
#include "s3log.h"
#include "s3utils.h"
#include "s3wrapper.h"

/* Do the module magic dance */

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(s3_export);
PG_FUNCTION_INFO_V1(s3_import);
PG_FUNCTION_INFO_V1(s3_validate_urls);

extern "C" {
Datum s3_export(PG_FUNCTION_ARGS);
Datum s3_import(PG_FUNCTION_ARGS);
Datum s3_validate_urls(PG_FUNCTION_ARGS);
}

#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
#define THREAD_ID pthread_self()

/* This array will store all of the mutexes available to OpenSSL. */
static MUTEX_TYPE *mutex_buf = NULL;

static void locking_function(int mode, int n, const char *file, int line) {
    if (mode & CRYPTO_LOCK)
        MUTEX_LOCK(mutex_buf[n]);
    else
        MUTEX_UNLOCK(mutex_buf[n]);
}

static unsigned long id_function(void) { return ((unsigned long)THREAD_ID); }

int thread_setup(void) {
    int i;

    mutex_buf =
        (pthread_mutex_t *)palloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));
    if (!mutex_buf) return 0;
    for (i = 0; i < CRYPTO_num_locks(); i++) MUTEX_SETUP(mutex_buf[i]);
    CRYPTO_set_id_callback(id_function);
    CRYPTO_set_locking_callback(locking_function);
    return 1;
}

int thread_cleanup(void) {
    int i;

    if (!mutex_buf) return 0;
    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    for (i = 0; i < CRYPTO_num_locks(); i++) MUTEX_CLEANUP(mutex_buf[i]);
    pfree(mutex_buf);
    mutex_buf = NULL;
    return 1;
}

/*
 * Import data into GPDB.
 * invoked by GPDB, be careful with C++ exceptions.
 */
Datum s3_import(PG_FUNCTION_ARGS) {
    S3ExtBase *myData;
    char *data;
    int data_len;
    size_t nread = 0;

    /* Must be called via the external table format manager */
    if (!CALLED_AS_EXTPROTOCOL(fcinfo))
        elog(ERROR,
             "extprotocol_import: not called by external protocol manager");

    /* Get our internal description of the protocol */
    myData = (S3ExtBase *)EXTPROTOCOL_GET_USER_CTX(fcinfo);

    if (EXTPROTOCOL_IS_LAST_CALL(fcinfo)) {
        if (myData) {
            thread_cleanup();
            if (!myData->Destroy()) {
                ereport(ERROR, (0, errmsg("Failed to cleanup S3 extention")));
            }
            delete myData;
        }

        /*
         * Cleanup function for the XML library.
         */
        xmlCleanupParser();

        PG_RETURN_INT32(0);
    }

    if (myData == NULL) {
        /* first call. do any desired init */
        curl_global_init(CURL_GLOBAL_ALL);
        thread_setup();

        char *url_with_options = EXTPROTOCOL_GET_URL(fcinfo);
        char *url = truncate_options(url_with_options);

        char *config_path = get_opt_s3(url_with_options, "config");
        if (!config_path) {
            // no config path in url, use default value
            // data_folder/gpseg0/s3/s3.conf
            config_path = strdup("s3/s3.conf");
        }

        bool result = InitConfig(config_path, "");
        if (!result) {
            free(config_path);
            ereport(ERROR, (0, errmsg("Can't find config file, please check")));
        } else {
            ClearConfig();
            free(config_path);
        }

        InitLog();

        if (s3ext_accessid == "") {
            ereport(ERROR, (0, errmsg("ERROR: access id is empty")));
        }

        if (s3ext_secret == "") {
            ereport(ERROR, (0, errmsg("ERROR: secret is empty")));
        }

        if ((s3ext_segnum == -1) || (s3ext_segid == -1)) {
            ereport(ERROR, (0, errmsg("ERROR: segment id is invalid")));
        }

        myData = CreateExtWrapper(url);

        if (!myData ||
            !myData->Init(s3ext_segid, s3ext_segnum, s3ext_chunksize)) {
            if (myData) delete myData;
            ereport(ERROR, (0, errmsg("Failed to init S3 extension, segid = "
                                      "%d, segnum = %d, please check your "
                                      "configurations and net connection",
                                      s3ext_segid, s3ext_segnum)));
        }

        EXTPROTOCOL_SET_USER_CTX(fcinfo, myData);

        free(url);
    }

    /* =======================================================================
     *                            DO THE IMPORT
     * =======================================================================
     */

    data = EXTPROTOCOL_GET_DATABUF(fcinfo);
    data_len = EXTPROTOCOL_GET_DATALEN(fcinfo);
    uint64_t readlen = 0;
    if (data_len > 0) {
        readlen = data_len;
        if (!myData->TransferData(data, readlen))
            ereport(ERROR, (0, errmsg("s3_import: could not read data")));
        nread = (size_t)readlen;
        // S3DEBUG("read %d data from S3", nread);
    }

    PG_RETURN_INT32((int)nread);
}

/*
 * Export data out of GPDB.
 * invoked by GPDB, be careful with C++ exceptions.
 */
Datum s3_export(PG_FUNCTION_ARGS) { PG_RETURN_INT32(0); }

Datum s3_validate_urls(PG_FUNCTION_ARGS) { PG_RETURN_VOID(); }
