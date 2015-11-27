/*-------------------------------------------------------------------------
 *
 * cdb_backup_status.h
 *
 * Structures and functions to read and write to cdb_backup_status table
 * using libpq
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDB_BACKUP_STATUS_H
#define CDB_BACKUP_STATUS_H

#define cdb_backup_status_schema "cdb"


/* --------------------------------------------------------------------------------------------------
 * Enum for the values of the task_id field of the cdb_backup_status table.
 * These need to stay in order where TASK_START < TASK_SET_SERIALIZABLE < TASK_FINISH
 * so the state machine processing will work properly.
 */
typedef enum backup_task_id
{
	TASK_START = 0,
	TASK_SET_SERIALIZABLE = 1,
	TASK_GOTLOCKS = 2,
	TASK_FINISH = 3,
	TASK_CANCEL = 4,
}	BackupTaskID;

/* --------------------------------------------------------------------------------------------------
 * Enum for the values of the task_rc field of the cdb_backup_status table
 */
typedef enum backup_task_rc
{
	TASK_RC_SUCCESS = 0,
	TASK_RC_FAILURE = 1
} BackupTaskRC;

/* --------------------------------------------------------------------------------------------------
 * struct for notification list items
 */
typedef struct status_op
{
	struct status_op *pNext;
	int			TaskID;
	int			TaskRC;
	char	   *pszSuffix;
	char	   *pszMsg;
}	StatusOp;

/* --------------------------------------------------------------------------------------------------
 * struct for notification list
 */
typedef struct status_op_list
{
	StatusOp   *pHead;
	pthread_mutex_t mGuard;
}	StatusOpList;

#define TASK_MSG_SUCCESS "Success"
#define SUFFIX_START "Start"
#define SUFFIX_SET_SERIALIZABLE "SetSerializable"
#define SUFFIX_GOTLOCKS "GotLocks"
#define SUFFIX_SUCCEED "Succeed"
#define SUFFIX_FAIL "Fail"
#define SUFFIX_MASTER_PROBE "MasterProbe"
#define SUFFIX_SEGMENT_PROBE "SegmentProbe"

/* Adds a StatusOp* to the list */
extern void AddToStatusOpList(StatusOpList * pList, StatusOp * pOp);

/* Creates a StatusOp*	*/
extern StatusOp *CreateStatusOp(int TaskID, int TaskRC, const char *pszSuffix, const char *pszMsg);

/* Creates a StatusOpList*	*/
extern StatusOpList *CreateStatusOpList(void);

/* Frees all the memory associated with a StatusOpList*  */
extern void DestroyStatusOpList(StatusOpList * pList);

/* Frees the memory allocated for the fields in a StatusOp*, and also the StatusOp* itself */
extern void FreeStatusOp(StatusOp * pOp);

/* Locks the mutex of the StatusOpList.  The mutex is created with the recursive attribute */
extern void Lock(StatusOpList * pList);

/* Removes the head from the linked list insider the StatusOpList object, and returns it. */
extern StatusOp *TakeFromStatusOpList(StatusOpList * pList);

/* Unlocks the mutex of the StatusOpList */
extern void Unlock(StatusOpList * pList);

#endif   /* CDB_BACKUP_STATUS_H */
