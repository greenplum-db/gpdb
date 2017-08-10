/*
 * This is a mock version of src/backend/access/transam/rmgr.c. The real
 * rmgr.c contains a table of the WAL redo/desc functions for all the WAL
 * record types. Leave out the table, so that we can leave out the AM
 * object files, which helps to cut down the size of the test programs.
 */
#include "postgres.h"

#include "access/rmgr.h"
#include "access/xlog_internal.h"

const RmgrData RmgrTable[RM_MAX_ID + 1];
