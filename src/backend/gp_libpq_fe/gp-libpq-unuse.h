/*
 * undef gp-libpq functions' names to allow using it with libpq in the same
 * process, check the README
 */

#ifdef GP_LIBPQ_USE_H
/*
 * gp-libpq-fe.h
 */

/* structs and typedefs */
#undef PGnotify
#undef PQprintOpt
#undef PQconninfoOption
#undef PQArgBlock
#undef PGresAttDesc
#undef PQaoRelTupCount

/* functions */
#undef PQconnectStart
#undef PQconnectStartParams
#undef PQconnectPoll
#undef PQconnectdb
#undef PQconnectdbParams
#undef PQsetdbLogin
#undef PQfinish
#undef PQconndefaults
#undef PQconninfoParse
#undef PQconninfoFree
#undef PQresetStart
#undef PQresetPoll
#undef PQreset
#undef PQgetCancel
#undef PQfreeCancel
#undef PQcancel
#undef PQrequestFinish
#undef PQrequestCancel
#undef PQdb
#undef PQuser
#undef PQpass
#undef PQhost
#undef PQport
#undef PQtty
#undef PQoptions
#undef PQstatus
#undef PQtransactionStatus
#undef PQparameterStatus
#undef PQprotocolVersion
#undef PQserverVersion
#undef PQerrorMessage
#undef PQsocket
#undef PQbackendPID
#undef PQconnectionNeedsPassword
#undef PQconnectionUsedPassword
#undef PQclientEncoding
#undef PQsetClientEncoding
#undef PQsetErrorVerbosity
#undef PQtrace
#undef PQuntrace
#undef PQsetNoticeReceiver
#undef PQsetNoticeProcessor
#undef PQregisterThreadLock
#undef PQexec
#undef PQexecParams
#undef PQprepare
#undef PQexecPrepared
#undef PQsendGpQuery_shared
#undef PQsendQuery
#undef PQsendQueryParams
#undef PQsendPrepare
#undef PQsendQueryPrepared
#undef PQsetSingleRowMode
#undef PQgetResult
#undef PQisBusy
#undef PQconsumeInput
#undef PQnotifies
#undef PQputCopyData
#undef PQputCopyEnd
#undef PQgetCopyData
#undef PQgetline
#undef PQputline
#undef PQgetlineAsync
#undef PQputnbytes
#undef PQendcopy
#undef PQsetnonblocking
#undef PQisnonblocking
#undef PQisthreadsafe
#undef PQping
#undef PQpingParams
#undef PQflush
#undef PQfn
#undef PQresultStatus
#undef PQresStatus
#undef PQresultErrorMessage
#undef PQresultErrorField
#undef PQntuples
#undef PQnfields
#undef PQbinaryTuples
#undef PQfname
#undef PQfnumber
#undef PQftable
#undef PQftablecol
#undef PQfformat
#undef PQftype
#undef PQfsize
#undef PQfmod
#undef PQcmdStatus
#undef PQoidStatus
#undef PQoidValue
#undef PQcmdTuples
#undef PQgetvalue
#undef PQgetlength
#undef PQgetisnull
#undef PQnparams
#undef PQparamtype
#undef PQdescribePrepared
#undef PQdescribePortal
#undef PQsendDescribePrepared
#undef PQsendDescribePortal
#undef PQclear
#undef PQfreemem
#undef PQmakeEmptyPGresult
#undef PQcopyResult
#undef PQsetResultAttrs
#undef PQresultAlloc
#undef PQsetvalue
#undef PQescapeStringConn
#undef PQescapeLiteral
#undef PQescapeIdentifier
#undef PQescapeByteaConn
#undef PQunescapeBytea
#undef PQescapeString
#undef PQescapeBytea
#undef PQprint
#undef PQdisplayTuples
#undef PQprintTuples
#undef lo_open
#undef lo_close
#undef lo_read
#undef lo_write
#undef lo_lseek
#undef lo_creat
#undef lo_create
#undef lo_tell
#undef lo_truncate
#undef lo_unlink
#undef lo_import
#undef lo_import_with_oid
#undef lo_export
#undef PQmblen
#undef PQdsplen
#undef PQenv2encoding
#undef PQprocessAoTupCounts
#undef PQencryptPassword


/*
 * gp-libpq-int.h
 */

/* structs and typedefs */
#undef pgCdbStatCell
#undef PQEnvironmentOption

/* functions */
#undef pqPacketSend
#undef pqGetHomeDirectory
#undef pqSetResultError
#undef pqCatenateResultError
#undef pqResultAlloc
#undef pqResultStrdup
#undef pqClearAsyncResult
#undef pqSaveErrorResult
#undef pqPrepareAsyncResult
#undef pqInternalNotice
#undef pqSaveMessageField
#undef pqSaveParameterStatus
#undef pqRowProcessor
#undef pqHandleSendFailure
#undef pqBuildStartupPacket3
#undef pqParseInput3
#undef pqGetErrorNotice3
#undef pqGetCopyData3
#undef pqGetline3
#undef pqGetlineAsync3
#undef pqEndcopy3
#undef pqFunctionCall3
#undef pqCheckOutBufferSpace
#undef pqCheckInBufferSpace
#undef pqGetc
#undef pqPutc
#undef pqGets
#undef pqGets_append
#undef pqPuts
#undef pqGetnchar
#undef pqSkipnchar
#undef pqPutnchar
#undef pqGetInt
#undef pqGetInt64
#undef pqPutInt
#undef pqPutMsgStart
#undef pqPutMsgEnd
#undef pqPutMsgEndNoAutoFlush
#undef pqReadData
#undef pqFlush
#undef pqFlushNonBlocking
#undef pqWait
#undef pqWaitTimed
#undef pqWaitTimeout
#undef pqReadReady
#undef pqWriteReady

#undef GP_LIBPQ_USE_H
#endif
