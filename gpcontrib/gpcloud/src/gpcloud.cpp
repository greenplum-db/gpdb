extern "C" {
#include "postgres.h"

/*
 * Spinlocks are in PostgreSQL defined with the register storage class,
 * which is perfectly legal C but which generates a warning when compiled
 * as C++. Ignore this warning as it's a false positive in this case.
 */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-register"
#endif

#include "access/external.h"
#include "access/extprotocol.h"
#include "access/xact.h"
#include "catalog/pg_proc.h"
#include "commands/defrem.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/resowner.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

/* Do the module magic dance */
PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(s3_export);
PG_FUNCTION_INFO_V1(s3_import);

Datum s3_export(PG_FUNCTION_ARGS);
Datum s3_import(PG_FUNCTION_ARGS);
}

#include "gpreader.h"
#include "gpwriter.h"

string s3extErrorMessage;

/*
 * To detect the query interruption event (triggered by user), we should
 * consider both QueryCancelPending variable and transaction status. Here
 * QueryCancelPending is not sufficient because it will be reset before the
 * extprotocol last call, then it is hard to distinguish normal exit/finish
 * from abnormal transaction abort.
 *
 * IsAbortInProgress() can also be reset by GPDB while downloading file, we
 * must save the state
 */
static bool queryCancelFlag = false;
bool S3QueryIsAbortInProgress(void) {
    bool queryIsBeingCancelled = QueryCancelPending || IsAbortInProgress();

    // We need a thread-safe way to query and set queryCancelFlag.
    // queryCancelFlag is set to TRUE for the first time when cancel signal is
    // detected. If cancel signal is already captured, value will not be
    // swapped and queryIsCancelledAlready is true.
    bool queryIsCancelledAlready =
        !__sync_bool_compare_and_swap(&queryCancelFlag, false, queryIsBeingCancelled);

    return queryIsBeingCancelled || queryIsCancelledAlready;
}

void *S3Alloc(size_t size) {
    return palloc(size);
}

/*
 * MemoryContext will be free automatically when query is completed or aborted.
 * So no need to call pfree() again unless it is really needed inside query execution.
 * Otherwise might free already cleaned memory context for failed query.
 *
 * Currently download/upload buffer are using memory from MemoryContext.
 */
void S3Free(void *p) {
    // pfree(p);
}

/*
 * Set up the thread signal mask, we don't want to run our signal handlers
 * in downloading and uploading threads.
 */
void MaskThreadSignals() {
    sigset_t sigs;

    if (pthread_equal(main_tid, pthread_self())) {
        S3ERROR("thread_mask is called from main thread!");
        return;
    }

    sigemptyset(&sigs);

    /* make our thread to ignore these signals (which should allow that they be
     * delivered to the main thread) */
    sigaddset(&sigs, SIGHUP);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGTERM);
    sigaddset(&sigs, SIGALRM);
    sigaddset(&sigs, SIGUSR1);
    sigaddset(&sigs, SIGUSR2);

    pthread_sigmask(SIG_BLOCK, &sigs, NULL);
}

/*
 * Detect data format
 * used to set file extension on S3 in gpwriter.
 */
static const char *getFormatStr(FunctionCallInfo fcinfo) {
    Relation rel = EXTPROTOCOL_GET_RELATION(fcinfo);
    ExtTableEntry *exttbl = GetExtTableEntry(rel->rd_id);
    char fmtcode = exttbl->fmtcode;

    if (fmttype_is_text(fmtcode)) return "txt";
    if (fmttype_is_csv(fmtcode)) return "csv";
    return S3_DEFAULT_FORMAT;
}

bool hasHeader = false;

char eolString[EOL_CHARS_MAX_LEN + 1] = "\n";  // LF by default

static void parseFormatOpts(FunctionCallInfo fcinfo) {
    Relation rel = EXTPROTOCOL_GET_RELATION(fcinfo);
    ExtTableEntry *exttbl = GetExtTableEntry(rel->rd_id);

    const char fmtcode = exttbl->fmtcode;
    const List *fmtopts = exttbl->options;
    char *newline_str = NULL;

    // only TEXT and CSV have detailed options
    if (fmttype_is_csv(fmtcode) || fmttype_is_text(fmtcode)) {
        ListCell *option;

        foreach (option, fmtopts) {
            DefElem *defel = (DefElem *)lfirst(option);
            if (strcmp(defel->defname, "header") == 0) {
                hasHeader = defGetBoolean(defel);
            } else if (strcmp(defel->defname, "newline") == 0) {
                newline_str = defGetString(defel);
            }
        }

        // default end-of-line terminator
        eolString[0] = '\n';
        eolString[1] = '\0';

        if (newline_str != NULL) {
            if (pg_strcasecmp(newline_str, "crlf") == 0) {
                eolString[0] = '\r';
                eolString[1] = '\n';
                eolString[2] = '\0';
            } else if (pg_strcasecmp(newline_str, "cr") == 0) {
                eolString[0] = '\r';
                eolString[1] = '\0';
            } else if (pg_strcasecmp(newline_str, "lf") == 0) {
                eolString[0] = '\n';
                eolString[1] = '\0';
            } else {  // should never come here
                ereport(ERROR, (0, errmsg("invalid value for NEWLINE (%s), "
                                          "valid options are: 'LF', 'CRLF', 'CR'",
                                          newline_str)));
            }
        }
    }
}

typedef struct gpcloudResHandle {
    GPReader *gpreader;
    GPWriter *gpwriter;

    ResourceOwner owner; /* owner of this handle */

    struct gpcloudResHandle *next;
    struct gpcloudResHandle *prev;
} gpcloudResHandle;

// Linked list of opened "handles", which are allocated in TopMemoryContext, and tracked by resource
// owners.
static gpcloudResHandle *openedResHandles;

static bool isGpcloudResReleaseCallbackRegistered;

static gpcloudResHandle *createGpcloudResHandle(void) {
    gpcloudResHandle *resHandle;

    resHandle = (gpcloudResHandle *)MemoryContextAlloc(TopMemoryContext, sizeof(gpcloudResHandle));
    resHandle->gpreader = NULL;
    resHandle->gpwriter = NULL;

    resHandle->owner = CurrentResourceOwner;
    resHandle->next = openedResHandles;
    resHandle->prev = NULL;

    if (openedResHandles) {
        openedResHandles->prev = resHandle;
    }

    openedResHandles = resHandle;

    return resHandle;
}

static void destroyGpcloudResHandle(gpcloudResHandle *resHandle) {
    if (resHandle == NULL) return;

    /* unlink from linked list first */
    if (resHandle->prev)
        resHandle->prev->next = resHandle->next;
    else
        openedResHandles = resHandle->next;

    if (resHandle->next) {
        resHandle->next->prev = resHandle->prev;
    }

    if (resHandle->gpreader != NULL) {
        if (!reader_cleanup(&resHandle->gpreader)) {
            elog(WARNING, "Failed to cleanup gpcloud extension: %s", s3extErrorMessage.c_str());
        }
    }

    if (resHandle->gpwriter != NULL) {
        if (!writer_cleanup(&resHandle->gpwriter)) {
            elog(WARNING, "Failed to cleanup gpcloud extension: %s", s3extErrorMessage.c_str());
        }
    }

    thread_cleanup();
    pfree(resHandle);
}

/*
 * Close any open handles on abort.
 */
static void gpcloudAbortCallback(ResourceReleasePhase phase, bool isCommit, bool isTopLevel,
                                 void *arg) {
    gpcloudResHandle *curr;
    gpcloudResHandle *next;

    if (phase != RESOURCE_RELEASE_AFTER_LOCKS) return;

    next = openedResHandles;
    while (next) {
        curr = next;
        next = curr->next;

        if (curr->owner == CurrentResourceOwner) {
            if (isCommit)
                elog(WARNING, "gpcloud external table reference leak: %p still referenced", curr);

            destroyGpcloudResHandle(curr);
        }
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
    gpcloudResHandle *resHandle = (gpcloudResHandle *)EXTPROTOCOL_GET_USER_CTX(fcinfo);

    /* last call. destroy reader */
    if (EXTPROTOCOL_IS_LAST_CALL(fcinfo)) {
        destroyGpcloudResHandle(resHandle);

        EXTPROTOCOL_SET_USER_CTX(fcinfo, NULL);
        PG_RETURN_INT32(0);
    }

    /* first call. do any desired init */
    if (resHandle == NULL) {
        if (!isGpcloudResReleaseCallbackRegistered) {
            RegisterResourceReleaseCallback(gpcloudAbortCallback, NULL);
            isGpcloudResReleaseCallbackRegistered = true;
        }
        resHandle = createGpcloudResHandle();

        queryCancelFlag = false;
        const char *url_with_options = EXTPROTOCOL_GET_URL(fcinfo);

        // has HEADER? and newline EOL?
        parseFormatOpts(fcinfo);

        thread_setup();

        resHandle->gpreader = reader_init(url_with_options);
        if (!resHandle->gpreader) {
            ereport(ERROR, (0, errmsg("Failed to init gpcloud extension (segid = %d, "
                                      "segnum = %d), please check your "
                                      "configurations and network connection: %s",
                                      s3ext_segid, s3ext_segnum, s3extErrorMessage.c_str())));
        }

        EXTPROTOCOL_SET_USER_CTX(fcinfo, resHandle);
    }

    char *data_buf = EXTPROTOCOL_GET_DATABUF(fcinfo);
    int32 data_len = EXTPROTOCOL_GET_DATALEN(fcinfo);

    if (!reader_transfer_data(resHandle->gpreader, data_buf, data_len)) {
        ereport(ERROR,
                (0, errmsg("s3_import: could not read data: %s", s3extErrorMessage.c_str())));
    }
    PG_RETURN_INT32(data_len);
}

/*
 * Export data out of GPDB.
 * invoked by GPDB, be careful with C++ exceptions.
 */
Datum s3_export(PG_FUNCTION_ARGS) {
    /* Must be called via the external table format manager */
    if (!CALLED_AS_EXTPROTOCOL(fcinfo))
        elog(ERROR, "extprotocol_import: not called by external protocol manager");

    /* Get our internal description of the protocol */
    gpcloudResHandle *resHandle = (gpcloudResHandle *)EXTPROTOCOL_GET_USER_CTX(fcinfo);

    /* last call. destroy writer */
    if (EXTPROTOCOL_IS_LAST_CALL(fcinfo)) {
        destroyGpcloudResHandle(resHandle);

        EXTPROTOCOL_SET_USER_CTX(fcinfo, NULL);
        PG_RETURN_INT32(0);
    }

    /* first call. do any desired init */
    if (resHandle == NULL) {
        if (!isGpcloudResReleaseCallbackRegistered) {
            RegisterResourceReleaseCallback(gpcloudAbortCallback, NULL);
            isGpcloudResReleaseCallbackRegistered = true;
        }
        resHandle = createGpcloudResHandle();

        queryCancelFlag = false;
        const char *url_with_options = EXTPROTOCOL_GET_URL(fcinfo);
        const char *format = getFormatStr(fcinfo);

        thread_setup();

        resHandle->gpwriter = writer_init(url_with_options, format);
        if (!resHandle->gpwriter) {
            ereport(ERROR, (0, errmsg("Failed to init gpcloud extension (segid = %d, "
                                      "segnum = %d), please check your "
                                      "configurations and network connection: %s",
                                      s3ext_segid, s3ext_segnum, s3extErrorMessage.c_str())));
        }

        EXTPROTOCOL_SET_USER_CTX(fcinfo, resHandle);
    }

    char *data_buf = EXTPROTOCOL_GET_DATABUF(fcinfo);
    int32 data_len = EXTPROTOCOL_GET_DATALEN(fcinfo);

    if (!writer_transfer_data(resHandle->gpwriter, data_buf, data_len)) {
        ereport(ERROR,
                (0, errmsg("s3_export: could not write data: %s", s3extErrorMessage.c_str())));
    }

    PG_RETURN_INT32(data_len);
}
