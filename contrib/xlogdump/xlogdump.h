/*
 * xlogdump.h
 *
 * Common header file for the xlogdump utility.
 */
#ifndef __XLOGDUMP_H__
#define __XLOGDUMP_H__

#define PRINT_XLOGRECORD_HEADER(X,Y) \
	printf("[cur:%X/%X, xid:%d, rmid:%d(%s), len/tot_len:%d/%d, info:%d, prev:%X/%X] ", \
	       (X).xlogid, (X).xrecoff, \
	       (Y)->xl_xid,		\
	       (Y)->xl_rmid,		\
	       RM_names[(Y)->xl_rmid],	\
	       (Y)->xl_len,		\
	       (Y)->xl_tot_len,		\
	       (Y)->xl_info,		\
	       (Y)->xl_prev.xlogid, (Y)->xl_prev.xrecoff)

struct transInfo
{
	TransactionId		xid;
	uint32			tot_len;
	int			status;
	struct transInfo	*next;
};

typedef struct transInfo transInfo;
typedef struct transInfo *transInfoPtr;

/* Transactions status used only with -t option */
static const char * const status_names[3] = {
	"NOT COMMITED",					/* 0 */
	"COMMITED    ",					/* 1 */
	"ABORTED     "
};

#endif /* __XLOGDUMP_H__ */
