#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "nodes/nodes.h"
#include "libpq/libpq.h"

/*
 * Mocked function of accept, because we cannot simulate accepting an
 * incoming connection in a unit test
 */
#define accept pqcomm_accept_mock
int pqcomm_accept_mock(int socket, struct sockaddr *restrict address,
		socklen_t *restrict address_len);

#include "../pqcomm.c"

/* Number of bytes requested to be sent through internal_flush */
#define TEST_NUM_BYTES 100

/* Number of seconds to set the SO_SNDTIMEO to */
#define SOCKET_TIMEOUT_SECONDS 1978

/* The mocked accept() function will return this socket when called */
static int test_accept_socket;

/*
 *  Test for internal_flush() for the case when:
 *    - requesting to send TEST_NUM_BYTES bytes
 *    - secure_write returns TEST_NUM_BYTES (send successful)
 *    - errno is not changed
 */
static void
test__internal_flush_successfulSend(void **state)
{
	int			result;

	expect_any(secure_write, port);
	expect_any(secure_write, ptr);
	expect_any(secure_write, len);
	will_return(secure_write, TEST_NUM_BYTES);

	PqSendPointer = TEST_NUM_BYTES;
	result = internal_flush();

	assert_int_equal(result,0);
	assert_int_equal(ClientConnectionLost, 0);
	assert_int_equal(InterruptPending, 0);
}

/*
 * Simulate side effects of secure_write. Sets the errno variable to val
 */
static void
_set_errno(void *val)
{
	errno = *((int *) val);
}

/*
 *  Test for internal_flush() for the case when:
 *    - secure_write returns 0 (send failed)
 *    - errno is set to EINTR
 */
static void
test__internal_flush_failedSendEINTR(void **state)
{
	int			result;

	/*
	 * In the case secure_write gets interrupted, we loop around and
	 * try the send again.
	 * In this test we simulate that, and secure_write will be called twice.
	 *
	 * First call to secure_write: returns 0 and sets errno to EINTR.
	 */
	expect_any(secure_write, port);
	expect_any(secure_write, ptr);
	expect_any(secure_write, len);
	static int errval = EINTR;
	will_return_with_sideeffect(secure_write, 0, _set_errno, &errval);

	/* Second call to secure_write: returns TEST_NUM_BYTES, i.e. success */
	expect_any(secure_write, port);
	expect_any(secure_write, ptr);
	expect_any(secure_write, len);
	will_return(secure_write, TEST_NUM_BYTES);

	PqSendPointer = TEST_NUM_BYTES;

	/* Call function under test */
	result = internal_flush();

	assert_int_equal(result,0);
	assert_int_equal(ClientConnectionLost, 0);
	assert_int_equal(InterruptPending, 0);
}

/*
 *  Test for internal_flush() for the case when:
 *    - secure_write returns 0 (send failed)
 *    - errno is set to EPIPE
 */
static void
test__internal_flush_failedSendEPIPE(void **state)
{
	int			result;

	/* Simulating that secure_write will fail, and set the errno to EPIPE */
	expect_any(secure_write, port);
	expect_any(secure_write, ptr);
	expect_any(secure_write, len);
	static int errval = EPIPE;
	will_return_with_sideeffect(secure_write, 0, _set_errno, &errval);

	/* In that case, we expect ereport(COMERROR, ...) to be called */
	expect_value(errstart, elevel, COMMERROR);
	expect_any(errstart, filename);
	expect_any(errstart, lineno);
	expect_any(errstart, funcname);
	expect_any(errstart, domain);
	will_return(errstart, false);

	PqSendPointer = TEST_NUM_BYTES;

	/* Call function under test */
	result = internal_flush();

	assert_int_equal(result,EOF);
	assert_int_equal(ClientConnectionLost, 1);
	assert_int_equal(InterruptPending, 1);
}

/*
 * This is a mocked version of the accept() system call
 * We don't actually accept an incoming connection, but we just return
 * a socket from the global variable test_accept_socket.
 */
int
pqcomm_accept_mock(int accept_sock, struct sockaddr *restrict address,
				   socklen_t *restrict address_len)
{
	return test_accept_socket;
}

/*
 * Test for StreamConnection that verifies that the socket has the SO_SNDTIMEO
 * timeout set for it when the connection is through a TCP/IP socket (AF_INET)
 */
static void
test__StreamConnection_set_SNDTIMEO_AF_INET(void **state)
{
	Port *port = (Port *) calloc(1, sizeof(Port));
	pgsocket server_fd = 1;

	GpIdentity.segindex = MASTER_CONTENT_ID;
	gp_connection_send_timeout = SOCKET_TIMEOUT_SECONDS;
	test_accept_socket = socket(AF_INET, SOCK_STREAM, 0 /* protocol */);

	int result = StreamConnection(server_fd, port);
	assert_int_equal(result, STATUS_OK);

	struct timeval timeout;
	socklen_t timeout_len = sizeof(timeout);
	result = getsockopt(port->sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, &timeout_len);
	/* Check that getsockopt ran successfully */
	assert_int_equal(result, 0);
	/* Check that the timeout is actually set */
	assert_int_equal(timeout.tv_sec, SOCKET_TIMEOUT_SECONDS);

	close(test_accept_socket);
}

/*
 * Test for StreamConnection that verifies that the socket has the SO_SNDTIMEO
 * timeout set for it when the connection is through a UNIX socket (AF_UNIX)
 */
static void
test__StreamConnection_set_SNDTIMEO_AF_UNIX(void **state)
{
	Port *port = (Port *) calloc(1, sizeof(Port));
	pgsocket server_fd = 1;

	GpIdentity.segindex = MASTER_CONTENT_ID;
	gp_connection_send_timeout = SOCKET_TIMEOUT_SECONDS;
	test_accept_socket = socket(AF_UNIX, SOCK_STREAM, 0 /* protocol */);

	int result = StreamConnection(server_fd, port);
	assert_int_equal(result, STATUS_OK);

	struct timeval timeout;
	socklen_t timeout_len = sizeof(timeout);
	result = getsockopt(port->sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, &timeout_len);

	/* Check that getsockopt ran successfully */
	assert_int_equal(result, 0);
	/* Check that the timeout is actually set */
	assert_int_equal(timeout.tv_sec, SOCKET_TIMEOUT_SECONDS);

	close(test_accept_socket);
}

/*
 * Test for StreamConnection that verifies that we don't set the socket
 * SO_SNDTIMEO timeout on segments
 */
static void
test__StreamConnection_set_SNDTIMEO_segment(void **state)
{
	Port *port = (Port *) calloc(1, sizeof(Port));
	pgsocket server_fd = 1;

	/* Use another segindex than master, anything != MASTER_CONTENT_ID will do */
	GpIdentity.segindex = 1;
	gp_connection_send_timeout = SOCKET_TIMEOUT_SECONDS;
	test_accept_socket = socket(AF_INET, SOCK_STREAM, 0 /* protocol */);

	int result = StreamConnection(server_fd, port);
	assert_int_equal(result, STATUS_OK);

	struct timeval timeout;
	socklen_t timeout_len = sizeof(timeout);
	result = getsockopt(port->sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, &timeout_len);

	/* Check that getsockopt ran successfully */
	assert_int_equal(result, 0);
	/* Check that the timeout is NOT actually set */
	assert_int_equal(timeout.tv_sec, 0);

	close(test_accept_socket);
}

/* ==================== main ==================== */
int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__internal_flush_successfulSend),
		unit_test(test__internal_flush_failedSendEINTR),
		unit_test(test__internal_flush_failedSendEPIPE),
		unit_test(test__StreamConnection_set_SNDTIMEO_AF_INET),
		unit_test(test__StreamConnection_set_SNDTIMEO_AF_UNIX),
		unit_test(test__StreamConnection_set_SNDTIMEO_segment)
	};

	return run_tests(tests);
}
