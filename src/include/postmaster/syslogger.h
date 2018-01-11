/*-------------------------------------------------------------------------
 *
 * syslogger.h
 *	  Exports from postmaster/syslogger.c.
 *
 * Copyright (c) 2004-2009, PostgreSQL Global Development Group
 *
 * $PostgreSQL: pgsql/src/include/postmaster/syslogger.h,v 1.15 2009/01/01 17:24:01 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef _SYSLOGGER_H
#define _SYSLOGGER_H

#include <limits.h>				/* for PIPE_BUF */

/*
 * We really want line-buffered mode for logfile output, but Windows does
 * not have it, and interprets _IOLBF as _IOFBF (bozos).  So use _IONBF
 * instead on Windows.
 */
#ifdef WIN32
#define LBF_MODE	_IONBF
#define LOG_EOL         "\r\n"
#else
#define LBF_MODE	_IOLBF
#define LOG_EOL         "\n"
#endif

/*
 * Primitive protocol structure for writing to syslogger pipe(s).  The idea
 * here is to divide long messages into chunks that are not more than
 * PIPE_BUF bytes long, which according to POSIX spec must be written into
 * the pipe atomically.  The pipe reader then uses the protocol headers to
 * reassemble the parts of a message into a single string.  The reader can
 * also cope with non-protocol data coming down the pipe, though we cannot
 * guarantee long strings won't get split apart.
 *
 * We use non-nul bytes in is_last to make the protocol a tiny bit
 * more robust against finding a false double nul byte prologue. But
 * we still might find it in the len and/or pid bytes unless we're careful.
 */

#ifdef PIPE_BUF
/* Are there any systems with PIPE_BUF > 64K?  Unlikely, but ... */
#if PIPE_BUF > 65536
#define PIPE_CHUNK_SIZE  65536
#else
#define PIPE_CHUNK_SIZE  ((int) PIPE_BUF)
#endif
#else							/* not defined */
/* POSIX says the value of PIPE_BUF must be at least 512, so use that */
#define PIPE_CHUNK_SIZE  512
#endif

/*
 * This is used to fake the thread id to be used inside the SEGV/BUS/ILL
 * handler.
 */
#define FIXED_THREAD_ID  123456

typedef struct 
{
	int32		zero;			/* leading zero */
	int32		len;			/* len, not including hdr */
	int32		pid;			/* writer's pid */
	int32       thid;			/* thread id */
	int32		main_thid;		/* main thread id */
	int32		chunk_no;		/* chunk number */
	char		is_last;		/* last chunk of message? 't' or 'f' */
	char		log_format;		/* 'c' for csv, 't' for text */
	char		is_segv_msg;	/* indicate whether this is a message sent in SEGV/BUS/ILL handler */
	int64		log_line_number;	/* indicate the order of the message */
	int64		next;			/* next chained chunk.  also force an 8 bytes align */
} PipeProtoHeader;

#define PIPE_HEADER_UNALIGNED_SIZE  sizeof(PipeProtoHeader)
#define PIPE_MAX_PAYLOAD  ((int) (PIPE_CHUNK_SIZE - MAXALIGN(PIPE_HEADER_UNALIGNED_SIZE)))

#define CHUNK_SLOTS 400

typedef struct 
{
	PipeProtoHeader hdr; 
	char		data[PIPE_MAX_PAYLOAD];
} PipeProtoChunk;

#define PIPE_HEADER_SIZE offsetof(PipeProtoChunk, data)

typedef struct CSVChunkStr
{
    const PipeProtoChunk *chunk;
    const char *p;
} CSVChunkStr;

extern void write_syslogger_file_binary(const char *buffer, int count, int dest);
extern void syslogger_log_chunk_list(PipeProtoChunk *chunk);

typedef struct
{
	pg_time_t session_start_time;
	char send_alert;
	char omit_location;
	char gp_is_primary;
	int32 gp_session_id;
	int32 gp_command_count;
	int32 gp_segment_id;
	int32 slice_id;
	int32 error_cursor_pos;
	int32 internal_query_pos;
	int32 error_fileline;
	TransactionId top_trans_id;
	DistributedTransactionId dist_trans_id;
	TransactionId local_trans_id;
	TransactionId subtrans_id;
} GpErrorDataFixFields;

/*
 * The format for GPDB error data.
 */
typedef struct
{
	/* Fix-length field */
	GpErrorDataFixFields fix_fields;

	/* variable-length field */
	char *username;
	char *databasename;
	char *remote_host;
	char *remote_port;
	const char *error_severity;
	char *sql_state;
	char *error_message;
	char *error_detail;
	char *error_hint;
	char *internal_query;
	char *error_context;
	char *debug_query_string;
	const char *error_func_name;
	const char *error_filename;
	char *stacktrace;
} GpErrorData;

/*
 * The format of GPDB segv/bus/ill error data.
 *
 * This structure contains minimal but essential information when a SEGV/BUS/ILL
 * signal is received. Note that the stack addresses are not stored here, but when
 * in use, they usually are stored right after this structure.
 *
 * No other data with variable-length are stored here currently, since we want to
 * minimize the work inside a signal handler.
 */
typedef struct
{
	pg_time_t session_start_time;
	int32 gp_session_id;
	int32 gp_command_count;
	int32 gp_segment_id;
	int32 slice_id;
	int32 signal_num;

	/* The depth of stack frame addresses that are stored after this structure */
	int32 frame_depth;
} GpSegvErrorData;

/* GUC options */
extern bool Logging_collector;
extern int	Log_RotationAge;
extern int	Log_RotationSize;
extern PGDLLIMPORT char *Log_directory;
extern PGDLLIMPORT char *Log_filename;
extern bool Log_truncate_on_rotation;
extern int gp_log_format;

extern bool am_syslogger;

#ifndef WIN32
extern int	syslogPipe[2];
#else
extern HANDLE syslogPipe[2];
#endif


extern int	SysLogger_Start(void);

extern void write_syslogger_file(const char *buffer, int count, int dest);

extern void syslogger_append_timestamp(pg_time_t stamp_time, bool amsyslogger, bool append_comma);
extern void syslogger_append_current_timestamp(bool amsyslogger);
extern void syslogger_write_int32(bool test0, const char *prefix, int32 i,
								  bool amsyslogger, bool append_comma);
extern int syslogger_write_str(const char *data, int len, bool amsyslogger, bool csv);

#ifdef EXEC_BACKEND
extern void SysLoggerMain(int argc, char *argv[]);
#endif

#endif   /* _SYSLOGGER_H */
