#include <unistd.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../../motion/ic_udpifc.c"

bool break_loop = false;

/*
 * PROTOTYPES
 */

extern ssize_t __real_sendto(int sockfd, const void *buf, size_t len, int flags,
							 const struct sockaddr *dest_addr, socklen_t addrlen);
int __wrap_errcode(int sqlerrcode);
int __wrap_errdetail(const char *fmt,...);
int __wrap_errmsg(const char *fmt,...);
ssize_t __wrap_sendto(int sockfd, const void *buf, size_t len, int flags,
					  const struct sockaddr *dest_addr, socklen_t addrlen);
void __wrap_elog_finish(int elevel, const char *fmt,...);
void __wrap_elog_start(const char *filename, int lineno, const char *funcname);
void __wrap_errfinish(int dummy __attribute__((unused)),...);
void __wrap_errstart(int elevel, const char *filename, int lineno, const char *funcname, const char *domain);
void __wrap_write_log(const char *fmt,...);

/*
 * WRAPPERS
 */

int __wrap_errcode(int sqlerrcode)  {return 0;}
int __wrap_errdetail(const char *fmt,...) { return 0; }
int __wrap_errmsg(const char *fmt,...) { return 0; }
void __wrap_elog_start(const char *filename, int lineno, const char *funcname) {}
void __wrap_errfinish(int dummy __attribute__((unused)),...) {}
void __wrap_errstart(int elevel, const char *filename, int lineno, const char *funcname, const char *domain){}

void __wrap_write_log(const char *fmt,...)
{
	/* check if we actually receive the message that sends the error */
	if (strcmp("Interconnect error: short conn receive (\%d)", fmt) == 0)
		break_loop = true;
}

void __wrap_elog_finish(int elevel, const char *fmt,...)
{
	assert_true(elevel <= LOG);
}

ssize_t __wrap_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
	assert_true(sockfd != PGINVALID_SOCKET);
#if defined(__darwin__)
	/* check to see if we converted the wildcard value to something routeable */
	if (udp_dummy_packet_sockaddr.ss_family == AF_INET6)
	{
		const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *) dest_addr;
		char address[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &in6->sin6_addr, address, sizeof(address));
		/* '::' and '::1' should always be '::1' */
		assert_true(strcmp("::1", address) == 0);
	}
#endif

	return	__real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

/*
 * HELPER FUNCTIONS
 */

static void wait_for_receiver(bool should_fail)
{
	int counter = 0;
	/* break_loop should be reset at the beginning of each test
	 * The while loop will end early once __wrap_write_log is called;
	 * this should happen when the receiver polls the message that
	 * SendDummyPacket sends.
	 */
	while(!break_loop)
	{
		/* we are sleeping for a generous amount of time; we should never
		 * need this much time. There is something wrong if it takes this long.
		 *
		 * expect to fail if the communication is invalid, i.e,. IPv4 to IPv6
		 */
		if (counter > 2)
			break;
		sleep(1);
		counter++;
	}

	if (should_fail)
		assert_true(counter > 2);
	else
		assert_true(counter < 2);
}

static void
start_receiver()
{
	pthread_attr_t	t_atts;
	sigset_t		pthread_sigs;
	int				pthread_err;

	pthread_attr_init(&t_atts);
	pthread_attr_setstacksize(&t_atts, Max(PTHREAD_STACK_MIN, (128 * 1024)));
	ic_set_pthread_sigmasks(&pthread_sigs);
	pthread_err = pthread_create(&ic_control_info.threadHandle, &t_atts, rxThreadFunc, NULL);
	ic_reset_pthread_sigmasks(&pthread_sigs);

	pthread_attr_destroy(&t_atts);
	if (pthread_err != 0)
	{
		ic_control_info.threadCreated = false;
		printf("failed to create thread");
		fail();
	}

	ic_control_info.threadCreated = true;
}

static sa_family_t
create_sender_socket(sa_family_t af)
{
	int sockfd = socket(af,
						SOCK_DGRAM,
						0);
	if (sockfd < 0)
	{
		printf("send dummy packet failed, create socket failed: %m\n");
		fail();
		return PGINVALID_SOCKET;
	}

	if (!pg_set_noblock(sockfd))
	{
		if (sockfd >= 0)
		{
			closesocket(sockfd);
		}
		printf("send dummy packet failed, setting socket with noblock failed: %m\n");
		fail();
		return PGINVALID_SOCKET;
	}

	return sockfd;
}

/*
 * START UNIT TEST
 */

static void
test_send_dummy_packet_ipv4_to_ipv4(void **state)
{
	break_loop = false;
	int listenerSocketFd;
	uint16 listenerPort;
	int txFamily;

	interconnect_address = "0.0.0.0";
	setupUDPListeningSocket(&listenerSocketFd, &listenerPort, &txFamily, &udp_dummy_packet_sockaddr);

	Gp_listener_port = (listenerPort << 16);
	UDP_listenerFd = listenerSocketFd;

	ICSenderSocket = create_sender_socket(AF_INET);
	ICSenderFamily = AF_INET;

	SendDummyPacket();

	const struct sockaddr_in *in = (const struct sockaddr_in *) &udp_dummy_packet_sockaddr;
	assert_true(txFamily == AF_INET);
	assert_true(in->sin_family == AF_INET);
	assert_true(listenerPort == ntohs(in->sin_port));
	assert_true(strcmp("0.0.0.0", inet_ntoa(in->sin_addr)) == 0);

	wait_for_receiver(false);
}

/* Sending from IPv4 to receiving on IPv6 is currently not supported.
 * The size of AF_INET6 is bigger than the side of IPv4, so converting from
 * IPv6 to IPv4 may potentially not work.
 */
static void
test_send_dummy_packet_ipv4_to_ipv6_should_fail(void **state)
{
	break_loop = false;
	int listenerSocketFd;
	uint16 listenerPort;
	int txFamily;

	interconnect_address = "::";
	setupUDPListeningSocket(&listenerSocketFd, &listenerPort, &txFamily, &udp_dummy_packet_sockaddr);

	Gp_listener_port = (listenerPort << 16);
	UDP_listenerFd = listenerSocketFd;

	ICSenderSocket = create_sender_socket(AF_INET);
	ICSenderFamily = AF_INET;

	SendDummyPacket();

	const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *) &udp_dummy_packet_sockaddr;
	assert_true(txFamily == AF_INET6);
	assert_true(in6->sin6_family == AF_INET6);
	assert_true(listenerPort == ntohs(in6->sin6_port));

	wait_for_receiver(true);
}

static void
test_send_dummy_packet_ipv6_to_ipv6(void **state)
{
	break_loop = false;
	int listenerSocketFd;
	uint16 listenerPort;
	int txFamily;

	interconnect_address = "::1";
	setupUDPListeningSocket(&listenerSocketFd, &listenerPort, &txFamily, &udp_dummy_packet_sockaddr);

	Gp_listener_port = (listenerPort << 16);
	UDP_listenerFd = listenerSocketFd;

	ICSenderSocket = create_sender_socket(AF_INET6);
	ICSenderFamily = AF_INET6;

	SendDummyPacket();

	const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *) &udp_dummy_packet_sockaddr;
	assert_true(txFamily == AF_INET6);
	assert_true(in6->sin6_family == AF_INET6);
	assert_true(listenerPort == ntohs(in6->sin6_port));

	wait_for_receiver(false);
}

static void
test_send_dummy_packet_ipv6_to_ipv4(void **state)
{
	break_loop = false;
	int listenerSocketFd;
	uint16 listenerPort;
	int txFamily;

	interconnect_address = "0.0.0.0";
	setupUDPListeningSocket(&listenerSocketFd, &listenerPort, &txFamily, &udp_dummy_packet_sockaddr);

	Gp_listener_port = (listenerPort << 16);
	UDP_listenerFd = listenerSocketFd;

	ICSenderSocket = create_sender_socket(AF_INET6);
	ICSenderFamily = AF_INET6;

	SendDummyPacket();

	const struct sockaddr_in *in = (const struct sockaddr_in *) &udp_dummy_packet_sockaddr;
	assert_true(txFamily == AF_INET);
	assert_true(in->sin_family == AF_INET);
	assert_true(listenerPort == ntohs(in->sin_port));
	assert_true(strcmp("0.0.0.0", inet_ntoa(in->sin_addr)) == 0);

	wait_for_receiver(false);
}


static void
test_send_dummy_packet_ipv6_to_ipv6_wildcard(void **state)
{
	break_loop = false;
	int listenerSocketFd;
	uint16 listenerPort;
	int txFamily;

	interconnect_address = "::";
	setupUDPListeningSocket(&listenerSocketFd, &listenerPort, &txFamily, &udp_dummy_packet_sockaddr);

	Gp_listener_port = (listenerPort << 16);
	UDP_listenerFd = listenerSocketFd;

	ICSenderSocket = create_sender_socket(AF_INET6);
	ICSenderFamily = AF_INET6;

	SendDummyPacket();

	const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *) &udp_dummy_packet_sockaddr;
	assert_true(txFamily == AF_INET6);
	assert_true(in6->sin6_family == AF_INET6);
	assert_true(listenerPort == ntohs(in6->sin6_port));

	wait_for_receiver(false);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	int is_ipv6_supported = true;
	int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
	if (sockfd < 0 && errno == EAFNOSUPPORT)
		is_ipv6_supported = false;

	log_min_messages = DEBUG1;

	start_receiver();

	if (is_ipv6_supported)
	{
		const UnitTest tests[] = {
			unit_test(test_send_dummy_packet_ipv4_to_ipv4),
			unit_test(test_send_dummy_packet_ipv4_to_ipv6_should_fail),
			unit_test(test_send_dummy_packet_ipv6_to_ipv6),
			unit_test(test_send_dummy_packet_ipv6_to_ipv4),
			unit_test(test_send_dummy_packet_ipv6_to_ipv6_wildcard),
		};
		return run_tests(tests);
	}
	else
	{
		printf("WARNING: IPv6 is not supported, skipping unittest\n");
		const UnitTest tests[] = {
			unit_test(test_send_dummy_packet_ipv4_to_ipv4),
		};
		return run_tests(tests);
	}
	return 0;
}
