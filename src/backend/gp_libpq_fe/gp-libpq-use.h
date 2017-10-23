/*
 * define libpq functions' names to allow using it with gp-libpq in the same
 * process, check the README
 */

#ifndef GP_LIBPQ_USE_H
#define GP_LIBPQ_USE_H

/*
 * gp-libpq-fe.h
 */

/* structs and typedefs */
#define PGnotify		gp_PGnotify
#define PQprintOpt		gp_PQprintOpt
#define PQconninfoOption		gp_PQconninfoOption
#define PQArgBlock		gp_PQArgBlock
#define PGresAttDesc		gp_PGresAttDesc
#define PQaoRelTupCount		gp_PQaoRelTupCount

/* functions */
#define PQconnectStart		gp_PQconnectStart
#define PQconnectStartParams		gp_PQconnectStartParams
#define PQconnectPoll		gp_PQconnectPoll
#define PQconnectdb		gp_PQconnectdb
#define PQconnectdbParams		gp_PQconnectdbParams
#define PQsetdbLogin		gp_PQsetdbLogin
#define PQfinish		gp_PQfinish
#define PQconndefaults		gp_PQconndefaults
#define PQconninfoParse		gp_PQconninfoParse
#define PQconninfoFree		gp_PQconninfoFree
#define PQresetStart		gp_PQresetStart
#define PQresetPoll		gp_PQresetPoll
#define PQreset		gp_PQreset
#define PQgetCancel		gp_PQgetCancel
#define PQfreeCancel		gp_PQfreeCancel
#define PQcancel		gp_PQcancel
#define PQrequestFinish		gp_PQrequestFinish
#define PQrequestCancel		gp_PQrequestCancel
#define PQdb		gp_PQdb
#define PQuser		gp_PQuser
#define PQpass		gp_PQpass
#define PQhost		gp_PQhost
#define PQport		gp_PQport
#define PQtty		gp_PQtty
#define PQoptions		gp_PQoptions
#define PQstatus		gp_PQstatus
#define PQtransactionStatus		gp_PQtransactionStatus
#define PQparameterStatus		gp_PQparameterStatus
#define PQprotocolVersion		gp_PQprotocolVersion
#define PQserverVersion		gp_PQserverVersion
#define PQerrorMessage		gp_PQerrorMessage
#define PQsocket		gp_PQsocket
#define PQbackendPID		gp_PQbackendPID
#define PQconnectionNeedsPassword		gp_PQconnectionNeedsPassword
#define PQconnectionUsedPassword		gp_PQconnectionUsedPassword
#define PQclientEncoding		gp_PQclientEncoding
#define PQsetClientEncoding		gp_PQsetClientEncoding
#define PQsetErrorVerbosity		gp_PQsetErrorVerbosity
#define PQtrace		gp_PQtrace
#define PQuntrace		gp_PQuntrace
#define PQsetNoticeReceiver		gp_PQsetNoticeReceiver
#define PQsetNoticeProcessor		gp_PQsetNoticeProcessor
#define PQregisterThreadLock		gp_PQregisterThreadLock
#define PQexec		gp_PQexec
#define PQexecParams		gp_PQexecParams
#define PQprepare		gp_PQprepare
#define PQexecPrepared		gp_PQexecPrepared
#define PQsendGpQuery_shared		gp_PQsendGpQuery_shared
#define PQsendQuery		gp_PQsendQuery
#define PQsendQueryParams		gp_PQsendQueryParams
#define PQsendPrepare		gp_PQsendPrepare
#define PQsendQueryPrepared		gp_PQsendQueryPrepared
#define PQsetSingleRowMode		gp_PQsetSingleRowMode
#define PQgetResult		gp_PQgetResult
#define PQisBusy		gp_PQisBusy
#define PQconsumeInput		gp_PQconsumeInput
#define PQnotifies		gp_PQnotifies
#define PQputCopyData		gp_PQputCopyData
#define PQputCopyEnd		gp_PQputCopyEnd
#define PQgetCopyData		gp_PQgetCopyData
#define PQgetline		gp_PQgetline
#define PQputline		gp_PQputline
#define PQgetlineAsync		gp_PQgetlineAsync
#define PQputnbytes		gp_PQputnbytes
#define PQendcopy		gp_PQendcopy
#define PQsetnonblocking		gp_PQsetnonblocking
#define PQisnonblocking		gp_PQisnonblocking
#define PQisthreadsafe		gp_PQisthreadsafe
#define PQping		gp_PQping
#define PQpingParams		gp_PQpingParams
#define PQflush		gp_PQflush
#define PQfn		gp_PQfn
#define PQresultStatus		gp_PQresultStatus
#define PQresStatus		gp_PQresStatus
#define PQresultErrorMessage		gp_PQresultErrorMessage
#define PQresultErrorField		gp_PQresultErrorField
#define PQntuples		gp_PQntuples
#define PQnfields		gp_PQnfields
#define PQbinaryTuples		gp_PQbinaryTuples
#define PQfname		gp_PQfname
#define PQfnumber		gp_PQfnumber
#define PQftable		gp_PQftable
#define PQftablecol		gp_PQftablecol
#define PQfformat		gp_PQfformat
#define PQftype		gp_PQftype
#define PQfsize		gp_PQfsize
#define PQfmod		gp_PQfmod
#define PQcmdStatus		gp_PQcmdStatus
#define PQoidStatus		gp_PQoidStatus
#define PQoidValue		gp_PQoidValue
#define PQcmdTuples		gp_PQcmdTuples
#define PQgetvalue		gp_PQgetvalue
#define PQgetlength		gp_PQgetlength
#define PQgetisnull		gp_PQgetisnull
#define PQnparams		gp_PQnparams
#define PQparamtype		gp_PQparamtype
#define PQdescribePrepared		gp_PQdescribePrepared
#define PQdescribePortal		gp_PQdescribePortal
#define PQsendDescribePrepared		gp_PQsendDescribePrepared
#define PQsendDescribePortal		gp_PQsendDescribePortal
#define PQclear		gp_PQclear
#define PQfreemem		gp_PQfreemem
#define PQmakeEmptyPGresult		gp_PQmakeEmptyPGresult
#define PQcopyResult		gp_PQcopyResult
#define PQsetResultAttrs		gp_PQsetResultAttrs
#define PQresultAlloc		gp_PQresultAlloc
#define PQsetvalue		gp_PQsetvalue
#define PQescapeStringConn		gp_PQescapeStringConn
#define PQescapeLiteral		gp_PQescapeLiteral
#define PQescapeIdentifier		gp_PQescapeIdentifier
#define PQescapeByteaConn		gp_PQescapeByteaConn
#define PQunescapeBytea		gp_PQunescapeBytea
#define PQescapeString		gp_PQescapeString
#define PQescapeBytea		gp_PQescapeBytea
#define PQprint		gp_PQprint
#define PQdisplayTuples		gp_PQdisplayTuples
#define PQprintTuples		gp_PQprintTuples
#define lo_open		gp_lo_open
#define lo_close		gp_lo_close
#define lo_read		gp_lo_read
#define lo_write		gp_lo_write
#define lo_lseek		gp_lo_lseek
#define lo_creat		gp_lo_creat
#define lo_create		gp_lo_create
#define lo_tell		gp_lo_tell
#define lo_truncate		gp_lo_truncate
#define lo_unlink		gp_lo_unlink
#define lo_import		gp_lo_import
#define lo_import_with_oid		gp_lo_import_with_oid
#define lo_export		gp_lo_export
#define PQmblen		gp_PQmblen
#define PQdsplen		gp_PQdsplen
#define PQenv2encoding		gp_PQenv2encoding
#define PQprocessAoTupCounts		gp_PQprocessAoTupCounts
#define PQencryptPassword		gp_PQencryptPassword


/*
 * gp-libpq-int.h
 */

/* structs and typedefs */
#define pgCdbStatCell		gp_pgCdbStatCell
#define PQEnvironmentOption		gp_PQEnvironmentOption

/* functions */
#define pqPacketSend		gp_pqPacketSend
#define pqGetHomeDirectory		gp_pqGetHomeDirectory
#define pqSetResultError		gp_pqSetResultError
#define pqCatenateResultError		gp_pqCatenateResultError
#define pqResultAlloc		gp_pqResultAlloc
#define pqResultStrdup		gp_pqResultStrdup
#define pqClearAsyncResult		gp_pqClearAsyncResult
#define pqSaveErrorResult		gp_pqSaveErrorResult
#define pqPrepareAsyncResult		gp_pqPrepareAsyncResult
#define pqInternalNotice		gp_pqInternalNotice
#define pqSaveMessageField		gp_pqSaveMessageField
#define pqSaveParameterStatus		gp_pqSaveParameterStatus
#define pqRowProcessor		gp_pqRowProcessor
#define pqHandleSendFailure		gp_pqHandleSendFailure
#define pqBuildStartupPacket3		gp_pqBuildStartupPacket3
#define pqParseInput3		gp_pqParseInput3
#define pqGetErrorNotice3		gp_pqGetErrorNotice3
#define pqGetCopyData3		gp_pqGetCopyData3
#define pqGetline3		gp_pqGetline3
#define pqGetlineAsync3		gp_pqGetlineAsync3
#define pqEndcopy3		gp_pqEndcopy3
#define pqFunctionCall3		gp_pqFunctionCall3
#define pqCheckOutBufferSpace		gp_pqCheckOutBufferSpace
#define pqCheckInBufferSpace		gp_pqCheckInBufferSpace
#define pqGetc		gp_pqGetc
#define pqPutc		gp_pqPutc
#define pqGets		gp_pqGets
#define pqGets_append		gp_pqGets_append
#define pqPuts		gp_pqPuts
#define pqGetnchar		gp_pqGetnchar
#define pqSkipnchar		gp_pqSkipnchar
#define pqPutnchar		gp_pqPutnchar
#define pqGetInt		gp_pqGetInt
#define pqGetInt64		gp_pqGetInt64
#define pqPutInt		gp_pqPutInt
#define pqPutMsgStart		gp_pqPutMsgStart
#define pqPutMsgEnd		gp_pqPutMsgEnd
#define pqPutMsgEndNoAutoFlush		gp_pqPutMsgEndNoAutoFlush
#define pqReadData		gp_pqReadData
#define pqFlush		gp_pqFlush
#define pqFlushNonBlocking		gp_pqFlushNonBlocking
#define pqWait		gp_pqWait
#define pqWaitTimed		gp_pqWaitTimed
#define pqWaitTimeout		gp_pqWaitTimeout
#define pqReadReady		gp_pqReadReady
#define pqWriteReady		gp_pqWriteReady

#endif
