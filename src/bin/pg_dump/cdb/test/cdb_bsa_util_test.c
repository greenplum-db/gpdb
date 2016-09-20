#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"
#include "c.h"

#ifdef USE_NETBACKUP

#include "../cdb_bsa_util.c"

/* Mock of fread function */
size_t fread(void *restrict ptr, size_t size, size_t nitems,
       FILE *restrict stream) {
	return (size_t)mock();
}

/* Mock of feof function */
int feof(FILE *stream) {
	return (size_t)mock();
}

/* Mock of ferror function */
int ferror(FILE *stream) {
	return (size_t)mock();
}

/* Mock of BSAInit function. */
int BSAInit(BSA_Handle *bsaHandlePtr, BSA_SecurityToken *tokenPtr,
			BSA_ObjectOwner *objectOwnerPtr, char **environmentPtr) {
	return (int)mock();
}

/* Mock of BSABeginTxn function. */
int BSABeginTxn(BSA_Handle bsaHandle) {
	return (int)mock();
}

/* Mock of BSACreateObject function. */
int BSACreateObject(BSA_Handle bsaHandle, BSA_ObjectDescriptor
	*objectDescriptorPtr, BSA_DataBlock32 *dataBlockPtr) {
	return (int)mock();
}

/* Mock of BSASendData function. */
int BSASendData(BSA_Handle bsaHandle, BSA_DataBlock32 *dataBlockPtr) {
	return (int)mock();
}

/* Mock of BSAEndData function. */
int BSAEndData(BSA_Handle bsaHandle) {
	return (int)mock();
}

/* Mock of BSAEndTxn function. */
int BSAEndTxn(BSA_Handle bsaHandle, BSA_Vote vote) {
	return (int)mock();
}

/* Mock of BSATerminate function. */
int BSATerminate(BSA_Handle bsaHandle) {
	return (int)mock();
}

/* Mock of BSAQueryObject function. */
int BSAQueryObject(BSA_Handle bsaHandle, BSA_QueryDescriptor *queryDescriptorPtr,
	BSA_ObjectDescriptor *objectDescriptorPtr) {
	return (int)mock();
}

/* Mock of BSAGetObject function. */
int BSAGetObject(BSA_Handle bsaHandle, BSA_ObjectDescriptor
	*objectDescriptorPtr, BSA_DataBlock32 *dataBlockPtr) {
	return (int)mock();
}

/* Mock of BSAGetData function. */
int BSAGetData(BSA_Handle bsaHandle, BSA_DataBlock32 *dataBlockPtr) {
	return (int)mock();
}

/* Mock of BSAGetNextQueryObject function. */
int BSAGetNextQueryObject(BSA_Handle bsaHandle, BSA_ObjectDescriptor *objectDescriptorPtr) {
	return (int)mock();
}

/* Mock of BSADeleteObject function. */
int BSADeleteObject(BSA_Handle bsaHandle, BSA_UInt64 copyId) {
	return (int)mock();
}

/* Unit tests for initBSADumpSession() function */
void
test_initBSADumpSession1(void **state)
{
	int result = -1;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == 0);
}

void
test_initBSADumpSession2(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, BSA_RC_ABORT_SYSTEM_ERROR);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession3(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, BSA_RC_AUTHENTICATION_FAILURE);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession4(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, BSA_RC_INVALID_CALL_SEQUENCE);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession5(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, BSA_RC_INVALID_ENV);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession6(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, BSA_RC_NULL_ARGUMENT);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession7(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, NBBSA_RC_FEATURE_NOT_LICENSED);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession8(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, BSA_RC_VERSION_NOT_SUPPORTED);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession9(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession10(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaKeyword = NULL;
	char* nbbsaSchedule = "test_schedule";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession11(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, NBBSA_RC_FEATURE_NOT_LICENSED);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession12(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_INVALID_HANDLE);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession13(void **state)
{
	int result = -1;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = "hello";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == 0);
}

void
test_initBSADumpSession14(void **state)
{
	int result = -1;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = " ";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == 0);
}


void
test_initBSADumpSession_servicehost_len_too_long(void **state)
{
	int result = 0;
	char* bsaServiceHost = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa129";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession_policy_len_too_long(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa129";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = NULL;
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}
void
test_initBSADumpSession_schedule_len_too_long(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa129";
	char* nbbsaKeyword = NULL;
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession_keyword_len_too_long(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa129";
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == -1);
}

void
test_initBSADumpSession_servicehost_len_ok(void **state)
{
	int result = -1;
	char* bsaServiceHost = "netbackup-service-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa127";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = " ";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == 0);
}

void
test_initBSADumpSession_policy_len_ok(void **state)
{
	int result = -1;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "netbackup-policy-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa127";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = " ";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == 0);
}

void
test_initBSADumpSession_schedule_len_ok(void **state)
{
	int result = -1;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "netbackup-schedule-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa127";
	char* nbbsaKeyword = " ";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == 0);
}

void
test_initBSADumpSession_keyword_len_ok(void **state)
{
	int result = -1;
	char* bsaServiceHost = "mdw";
	char* nbbsaPolicy = "test_policy";
	char* nbbsaSchedule = "test_schedule";
	char* nbbsaKeyword = "netbackup-keyword-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa127";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_SUCCESS);
	result = initBSADumpSession(bsaServiceHost, nbbsaPolicy, nbbsaSchedule, nbbsaKeyword);
	assert_true(result == 0);
}
/* Unit tests for createBSADumpObject() function */

void
test_createBSADumpObject1(void **state)
{
	int result = -1;
	will_return(BSACreateObject, BSA_RC_SUCCESS);
	result = createBSADumpObject("gp_dump_20140219000000.rpt");
	assert_true(result == 0);
}

void
test_createBSADumpObject2(void **state)
{
	int result = 0;
	will_return(BSACreateObject, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = createBSADumpObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_createBSADumpObject3(void **state)
{
	int result = 0;
	will_return(BSACreateObject, BSA_RC_ACCESS_FAILURE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = createBSADumpObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_createBSADumpObject4(void **state)
{
	int result = 0;
	will_return(BSACreateObject, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = createBSADumpObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_createBSADumpObject5(void **state)
{
	int result = 0;
	will_return(BSACreateObject, BSA_RC_INVALID_DATABLOCK);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = createBSADumpObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_createBSADumpObject6(void **state)
{
	int result = 0;
	will_return(BSACreateObject, BSA_RC_INVALID_HANDLE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = createBSADumpObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_createBSADumpObject7(void **state)
{
	int result = 0;
	will_return(BSACreateObject, BSA_RC_INVALID_OBJECTDESCRIPTOR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = createBSADumpObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_createBSADumpObject8(void **state)
{
	int result = 0;
	will_return(BSACreateObject, BSA_RC_NULL_ARGUMENT);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = createBSADumpObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

/* Unit tests for sendBSADumpData() function */

void
test_sendBSADumpData1(void **state)
{
	int result = -1;
	int netbackup_block_size = 512;
	will_return(fread, 10);
	will_return(BSASendData, BSA_RC_SUCCESS);
	will_return(feof, 1);
	result = sendBSADumpData(netbackup_block_size);
	assert_true(result == 0);
}

void
test_sendBSADumpData2(void **state)
{
	int result = 0;
	int netbackup_block_size = 512;
	will_return(fread, 10);
	will_return(BSASendData, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = sendBSADumpData(netbackup_block_size);
	assert_true(result == -1);
}

void
test_sendBSADumpData3(void **state)
{
	int result = 0;
	int netbackup_block_size = 512;
	will_return(fread, 10);
	will_return(BSASendData, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = sendBSADumpData(netbackup_block_size);
	assert_true(result == -1);
}

void
test_sendBSADumpData4(void **state)
{
	int result = 0;
	int netbackup_block_size = 512;
	will_return(fread, 10);
	will_return(BSASendData, BSA_RC_INVALID_DATABLOCK);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = sendBSADumpData(netbackup_block_size);
	assert_true(result == -1);
}

void
test_sendBSADumpData5(void **state)
{
	int result = 0;
	int netbackup_block_size = 512;
	will_return(fread, 10);
	will_return(BSASendData, BSA_RC_INVALID_HANDLE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = sendBSADumpData(netbackup_block_size);
	assert_true(result == -1);
}

void
test_sendBSADumpData6(void **state)
{
	int result = 0;
	int netbackup_block_size = 512;
	will_return(fread, 10);
	will_return(BSASendData, BSA_RC_NULL_ARGUMENT);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = sendBSADumpData(netbackup_block_size);
	assert_true(result == -1);
}

void
test_sendBSADumpData7(void **state)
{
	int result = 0;
	int netbackup_block_size = 512;
	will_return(fread, 10);
	will_return(BSASendData, BSA_RC_SUCCESS);
	will_return(feof, 0);
	will_return(ferror, 1);
	result = sendBSADumpData(netbackup_block_size);
	assert_true(result == -1);
}

/* Unit tests for endBSADumpSession() function */

void
test_endBSADumpSession1(void **state)
{
	int result = -1;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSADumpSession();
	assert_true(result == 0);
}

void
test_endBSADumpSession2(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSADumpSession();
	assert_true(result == -1);
}

void
test_endBSADumpSession3(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSADumpSession();
	assert_true(result == -1);
}

void
test_endBSADumpSession4(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_INVALID_HANDLE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSADumpSession();
	assert_true(result == -1);
}

void
test_endBSADumpSession5(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSADumpSession();
	assert_true(result == -1);
}

void
test_endBSADumpSession6(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSADumpSession();
	assert_true(result == -1);
}

void
test_endBSADumpSession7(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_INVALID_HANDLE);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSADumpSession();
	assert_true(result == -1);
}

void
test_endBSADumpSession8(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_INVALID_VOTE);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSADumpSession();
	assert_true(result == -1);
}

void
test_endBSADumpSession9(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_TRANSACTION_ABORTED);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSADumpSession();
	assert_true(result == -1);
}

/* Unit tests for initBSARestoreSession() function */

void
test_initBSARestoreSession1(void **state)
{
	int result = -1;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_SUCCESS);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == 0);
}

void
test_initBSARestoreSession2(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_ABORT_SYSTEM_ERROR);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession3(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_AUTHENTICATION_FAILURE);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession4(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_INVALID_CALL_SEQUENCE);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession5(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_INVALID_ENV);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession6(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_NULL_ARGUMENT);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession7(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, NBBSA_RC_FEATURE_NOT_LICENSED);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession8(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_VERSION_NOT_SUPPORTED);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession9(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession10(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession11(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, NBBSA_RC_FEATURE_NOT_LICENSED);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession12(void **state)
{
	int result = 0;
	char* bsaServiceHost = "mdw";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_INVALID_HANDLE);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession_servicehost_len_too_long(void **state)
{
	int result = 0;
	char* bsaServiceHost = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa129";
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == -1);
}

void
test_initBSARestoreSession_servicehost_len_ok(void **state)
{
	int result = -1;
	char* bsaServiceHost = "netbackup-service-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa127";
	will_return(BSAInit, BSA_RC_SUCCESS);
	will_return(BSABeginTxn, BSA_RC_SUCCESS);
	result = initBSARestoreSession(bsaServiceHost);
	assert_true(result == 0);
}

/* Unit tests for getBSARestoreObject() function */

void
test_getBSARestoreObject1(void **state)
{
	int result = -1;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetObject, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == 0);
}

void
test_getBSARestoreObject2(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject3(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_ACCESS_FAILURE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject4(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject5(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_INVALID_HANDLE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject6(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_INVALID_QUERYDESCRIPTOR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject7(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_NO_MATCH);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject8(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_NULL_ARGUMENT);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject9(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetObject, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject10(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetObject, BSA_RC_ACCESS_FAILURE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject11(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetObject, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject12(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetObject, BSA_RC_INVALID_COPYID);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject13(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetObject, BSA_RC_INVALID_HANDLE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject14(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetObject, BSA_RC_NULL_ARGUMENT);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

void
test_getBSARestoreObject15(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetObject, BSA_RC_OBJECT_NOT_FOUND);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreObject("gp_dump_20140219000000.rpt");
	assert_true(result == -1);
}

/* Unit tests for getBSARestoreData() function */

void
test_getBSARestoreData1(void **state)
{
	int result = -1;
	int NetBackupBlockSize = 512;
	will_return(BSAGetData, BSA_RC_NO_MORE_DATA);
	result = getBSARestoreData(NetBackupBlockSize);
	assert_true(result == 0);
}

void
test_getBSARestoreData2(void **state)
{
	int result = 0;
	int NetBackupBlockSize = 512;
	will_return(BSAGetData, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreData(NetBackupBlockSize);
	assert_true(result == -1);
}

void
test_getBSARestoreData3(void **state)
{
	int result = 0;
	int NetBackupBlockSize = 512;
	will_return(BSAGetData, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreData(NetBackupBlockSize);
	assert_true(result == -1);
}

void
test_getBSARestoreData4(void **state)
{
	int result = 0;
	int NetBackupBlockSize = 1024;
	will_return(BSAGetData, BSA_RC_INVALID_DATABLOCK);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreData(NetBackupBlockSize);
	assert_true(result == -1);
}

void
test_getBSARestoreData5(void **state)
{
	int result = 0;
	int NetBackupBlockSize = 1024;
	will_return(BSAGetData, BSA_RC_INVALID_HANDLE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreData(NetBackupBlockSize);
	assert_true(result == -1);
}

void
test_getBSARestoreData6(void **state)
{
	int result = 0;
	int NetBackupBlockSize = 1024;
	will_return(BSAGetData, BSA_RC_NULL_ARGUMENT);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = getBSARestoreData(NetBackupBlockSize);
	assert_true(result == -1);
}

/* Unit tests for endBSARestoreSession() function */

void
test_endBSARestoreSession1(void **state)
{
	int result = -1;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSARestoreSession();
	assert_true(result == 0);
}

void
test_endBSARestoreSession2(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSARestoreSession();
	assert_true(result == -1);
}

void
test_endBSARestoreSession3(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSARestoreSession();
	assert_true(result == -1);
}

void
test_endBSARestoreSession4(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_INVALID_HANDLE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSARestoreSession();
	assert_true(result == -1);
}

void
test_endBSARestoreSession5(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSARestoreSession();
	assert_true(result == -1);
}

void
test_endBSARestoreSession6(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSARestoreSession();
	assert_true(result == -1);
}

void
test_endBSARestoreSession7(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_INVALID_HANDLE);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSARestoreSession();
	assert_true(result == -1);
}

void
test_endBSARestoreSession8(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_INVALID_VOTE);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSARestoreSession();
	assert_true(result == -1);
}

void
test_endBSARestoreSession9(void **state)
{
	int result = 0;
	will_return(BSAEndData, BSA_RC_SUCCESS);
	will_return(BSAEndTxn, BSA_RC_TRANSACTION_ABORTED);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = endBSARestoreSession();
	assert_true(result == -1);
}

void
test_queryBSAObject1(void **state)
{
	char *result = NULL;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	result = queryBSAObject("/tmp/gp_dump");
	assert_true(strcmp(result, "/tmp/gp_dump") == 0);
}

void
test_queryBSAObject2(void **state)
{
	char *result = NULL;
	will_return(BSAQueryObject, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = queryBSAObject("/tmp/gp_dump");
	assert_true(result == NULL);
}

void
test_queryBSAObject3(void **state)
{
	char *result = NULL;
	will_return(BSAQueryObject, BSA_RC_ACCESS_FAILURE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = queryBSAObject("/tmp/gp_dump");
	assert_true(result == NULL);
}

void
test_queryBSAObject4(void **state)
{
	char *result = NULL;
	will_return(BSAQueryObject, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = queryBSAObject("/tmp/gp_dump");
	assert_true(result == NULL);
}

void
test_queryBSAObject5(void **state)
{
	char *result = NULL;
	will_return(BSAQueryObject, BSA_RC_INVALID_HANDLE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = queryBSAObject("/tmp/gp_dump");
	assert_true(result == NULL);
}

void
test_queryBSAObject6(void **state)
{
	char *result = NULL;
	will_return(BSAQueryObject, BSA_RC_INVALID_QUERYDESCRIPTOR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = queryBSAObject("/tmp/gp_dump");
	assert_true(result == NULL);
}

void
test_queryBSAObject7(void **state)
{
	char *result = NULL;
	will_return(BSAQueryObject, BSA_RC_NO_MATCH);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = queryBSAObject("/tmp/gp_dump");
	assert_true(result == NULL);
}

void
test_queryBSAObject8(void **state)
{
	char *result = NULL;
	will_return(BSAQueryObject, BSA_RC_NULL_ARGUMENT);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = queryBSAObject("/tmp/gp_dump");
	assert_true(result == NULL);
}

void
test_searchBSAObject1(void **state)
{
	int result = -1;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_NO_MORE_DATA);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == 0);
}

void
test_searchBSAObject2(void **state)
{
	int result = -1;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_NO_MORE_DATA);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == 0);
}

void
test_searchBSAObject3(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == -1);
}

void
test_searchBSAObject4(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_ACCESS_FAILURE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == -1);
}

void
test_searchBSAObject5(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_NO_MATCH);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == -1);
}

void
test_searchBSAObject6(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == -1);
}

void
test_searchBSAObject7(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == -1);
}

void
test_searchBSAObject8(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == -1);
}

void
test_searchBSAObject9(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_INVALID_HANDLE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == -1);
}

void
test_searchBSAObject10(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_NULL_ARGUMENT);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == -1);
}

void
test_searchBSAObject11(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_NO_MORE_DATA);
	will_return(BSAEndTxn, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = searchBSAObject("/tmp/gp_dump*rpt");
	assert_true(result == -1);
}

void
test_deleteBSAObjects1(void **state)
{
	int result = -1;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_NO_MORE_DATA);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*gp_dump*");
	assert_true(result == 0);
}

void
test_deleteBSAObjects2(void **state)
{
	int result = -1;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_NO_MORE_DATA);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*gp_dump*");
	assert_true(result == 0);
}

void
test_deleteBSAObjects3(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*gp_dump*");
	assert_true(result == -1);
}

void
test_deleteBSAObjects4(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_ACCESS_FAILURE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*gp_dump*");
	assert_true(result == -1);
}

void
test_deleteBSAObjects5(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_INVALID_COPYID);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*gp_dump*");
	assert_true(result == -1);
}

void
test_deleteBSAObjects6(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_NO_MORE_DATA);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*gp_dump*");
	assert_true(result == 0);
}

void
test_deleteBSAObjects7(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*gp_dump*rpt");
	assert_true(result == -1);
}

void
test_deleteBSAObjects8(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_INVALID_CALL_SEQUENCE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*gp_dump*rpt");
	assert_true(result == -1);
}

void
test_deleteBSAObjects9(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_INVALID_HANDLE);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*gp_dump*rpt");
	assert_true(result == -1);
}

void
test_deleteBSAObjects10(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_NULL_ARGUMENT);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*foo*");
	assert_true(result == -1);
}

void
test_deleteBSAObjects11(void **state)
{
	int result = 0;
	will_return(BSAQueryObject, BSA_RC_SUCCESS);
	will_return(BSADeleteObject, BSA_RC_SUCCESS);
	will_return(BSAGetNextQueryObject, BSA_RC_NO_MORE_DATA);
	will_return(BSAEndTxn, BSA_RC_ABORT_SYSTEM_ERROR);
	will_return(BSAEndTxn, BSA_RC_SUCCESS);
	will_return(BSATerminate, BSA_RC_SUCCESS);
	result = deleteBSAObjects("*foo*");
	assert_true(result == -1);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test_initBSADumpSession1),
			unit_test(test_initBSADumpSession2),
			unit_test(test_initBSADumpSession3),
			unit_test(test_initBSADumpSession4),
			unit_test(test_initBSADumpSession5),
			unit_test(test_initBSADumpSession6),
			unit_test(test_initBSADumpSession7),
			unit_test(test_initBSADumpSession8),
			unit_test(test_initBSADumpSession9),
			unit_test(test_initBSADumpSession10),
			unit_test(test_initBSADumpSession11),
			unit_test(test_initBSADumpSession12),
			unit_test(test_initBSADumpSession13),
			unit_test(test_initBSADumpSession14),
			unit_test(test_initBSADumpSession_servicehost_len_too_long),
			unit_test(test_initBSADumpSession_policy_len_too_long),
			unit_test(test_initBSADumpSession_schedule_len_too_long),
			unit_test(test_initBSADumpSession_keyword_len_too_long),
			unit_test(test_initBSADumpSession_servicehost_len_ok),
			unit_test(test_initBSADumpSession_policy_len_ok),
			unit_test(test_initBSADumpSession_schedule_len_ok),
			unit_test(test_initBSADumpSession_keyword_len_ok),
			unit_test(test_createBSADumpObject1),
			unit_test(test_createBSADumpObject2),
			unit_test(test_createBSADumpObject3),
			unit_test(test_createBSADumpObject4),
			unit_test(test_createBSADumpObject5),
			unit_test(test_createBSADumpObject6),
			unit_test(test_createBSADumpObject7),
			unit_test(test_createBSADumpObject8),
			unit_test(test_sendBSADumpData1),
			unit_test(test_sendBSADumpData2),
			unit_test(test_sendBSADumpData3),
			unit_test(test_sendBSADumpData4),
			unit_test(test_sendBSADumpData5),
			unit_test(test_sendBSADumpData6),
			unit_test(test_sendBSADumpData7),
			unit_test(test_endBSADumpSession1),
			unit_test(test_endBSADumpSession2),
			unit_test(test_endBSADumpSession3),
			unit_test(test_endBSADumpSession4),
			unit_test(test_endBSADumpSession5),
			unit_test(test_endBSADumpSession6),
			unit_test(test_endBSADumpSession7),
			unit_test(test_endBSADumpSession8),
			unit_test(test_endBSADumpSession9),
			unit_test(test_initBSARestoreSession1),
			unit_test(test_initBSARestoreSession2),
			unit_test(test_initBSARestoreSession3),
			unit_test(test_initBSARestoreSession4),
			unit_test(test_initBSARestoreSession5),
			unit_test(test_initBSARestoreSession6),
			unit_test(test_initBSARestoreSession7),
			unit_test(test_initBSARestoreSession8),
			unit_test(test_initBSARestoreSession9),
			unit_test(test_initBSARestoreSession10),
			unit_test(test_initBSARestoreSession11),
			unit_test(test_initBSARestoreSession12),
			unit_test(test_initBSARestoreSession_servicehost_len_too_long),
			unit_test(test_initBSARestoreSession_servicehost_len_ok),
			unit_test(test_getBSARestoreObject1),
			unit_test(test_getBSARestoreObject2),
			unit_test(test_getBSARestoreObject3),
			unit_test(test_getBSARestoreObject4),
			unit_test(test_getBSARestoreObject5),
			unit_test(test_getBSARestoreObject6),
			unit_test(test_getBSARestoreObject7),
			unit_test(test_getBSARestoreObject8),
			unit_test(test_getBSARestoreObject9),
			unit_test(test_getBSARestoreObject10),
			unit_test(test_getBSARestoreObject11),
			unit_test(test_getBSARestoreObject12),
			unit_test(test_getBSARestoreObject13),
			unit_test(test_getBSARestoreObject14),
			unit_test(test_getBSARestoreObject15),
			unit_test(test_getBSARestoreData1),
			unit_test(test_getBSARestoreData2),
			unit_test(test_getBSARestoreData3),
			unit_test(test_getBSARestoreData4),
			unit_test(test_getBSARestoreData5),
			unit_test(test_getBSARestoreData6),
			unit_test(test_endBSARestoreSession1),
			unit_test(test_endBSARestoreSession2),
			unit_test(test_endBSARestoreSession3),
			unit_test(test_endBSARestoreSession4),
			unit_test(test_endBSARestoreSession5),
			unit_test(test_endBSARestoreSession6),
			unit_test(test_endBSARestoreSession7),
			unit_test(test_endBSARestoreSession8),
			unit_test(test_endBSARestoreSession9),
			unit_test(test_queryBSAObject1),
			unit_test(test_queryBSAObject2),
			unit_test(test_queryBSAObject3),
			unit_test(test_queryBSAObject4),
			unit_test(test_queryBSAObject5),
			unit_test(test_queryBSAObject6),
			unit_test(test_queryBSAObject7),
			unit_test(test_queryBSAObject8),
			unit_test(test_searchBSAObject1),
			unit_test(test_searchBSAObject2),
			unit_test(test_searchBSAObject3),
			unit_test(test_searchBSAObject4),
			unit_test(test_searchBSAObject5),
			unit_test(test_searchBSAObject6),
			unit_test(test_searchBSAObject7),
			unit_test(test_searchBSAObject8),
			unit_test(test_searchBSAObject9),
			unit_test(test_searchBSAObject10),
			unit_test(test_searchBSAObject11),
			unit_test(test_deleteBSAObjects1),
			unit_test(test_deleteBSAObjects2),
			unit_test(test_deleteBSAObjects3),
			unit_test(test_deleteBSAObjects4),
			unit_test(test_deleteBSAObjects5),
			unit_test(test_deleteBSAObjects6),
			unit_test(test_deleteBSAObjects7),
			unit_test(test_deleteBSAObjects8),
			unit_test(test_deleteBSAObjects9),
			unit_test(test_deleteBSAObjects10),
			unit_test(test_deleteBSAObjects11)
	};
	return run_tests(tests);
}

#else

int main(int argc, char *argv[])
{
	/* TODO: change printf() to mpp_err_msg() */
	printf("\ngp_bsa_dump_agent is not supported on this platform\n\n");
	return 0;
}

#endif
