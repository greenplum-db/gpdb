/*-------------------------------------------------------------------------
 *
 * cdbicudpfaultinjection.h
 *	   Fault injection code for UDP interconnect.
 *
 * Portions Copyright (c) 2005-2011, Greenplum Inc.
 * Portions Copyright (c) 2011-2012, EMC Corporation
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbicudpfaultinjection.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBICUDPFAULTINJECTION_H
#define CDBICUDPFAULTINJECTION_H

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#ifdef USE_ASSERT_CHECKING

static bool udp_testmode = false;

static inline void
set_test_mode()
{
	if (gp_udpic_dropseg != UNDEF_SEGMENT
			|| gp_udpic_dropacks_percent != 0
			|| gp_udpic_dropxmit_percent != 0
			|| gp_udpic_fault_inject_percent != 0)
		udp_testmode = true;
	else
		udp_testmode = false;
}

/*
 * testmode_inject_fault
 *		Return whether we inject a fault given a probability.
 *
 */
static inline bool
testmode_inject_fault(int percent)
{
	if (udp_testmode &&
		(gp_udpic_dropseg == UNDEF_SEGMENT || gp_udpic_dropseg == GpIdentity.segindex))
	{
			if (random() % 100 < percent)
				return true;
	}

	return false;
}

/* Track the malloc times */
static pthread_mutex_t	icudp_malloc_tracking_lock = PTHREAD_MUTEX_INITIALIZER;
static int64 icudp_malloc_times = 0;

/* Fault type enumeration. */
typedef enum {
	/* These are used to inject packet content corruption. */
	FINC_PKT_HEADER_SHORTEN = 0,
	FINC_PKT_PKT_SHORTEN = 1,
	FINC_PKT_CRC_CORRUPT = 2,
	FINC_PKT_HEADER_LEN_ZERO = 3,
	FINC_PKT_HEADER_LEN_NEGATIVE = 4,
	FINC_PKT_MISMATCH = 5,

	/* These are used to inject query cancel and process die. */
	FINC_INTR_QUERY_CANCEL = 12,
	FINC_INTR_PROC_DIE = 13,

	/* These are used to inject OS API errors. */
	FINC_OS_EAGAIN = 16,
	FINC_OS_EINTR = 17,
	FINC_OS_EWOULDBLOCK = 18,
	FINC_OS_NET_INTERFACE = 19,
	FINC_OS_MEM_INTERFACE = 20,
	FINC_OS_CREATE_THREAD = 21,

	/* These are used to inject network faults. */
	FINC_NET_PKT_DUP = 24,
	FINC_NET_RECV_ZERO = 25,

	/* This is a fault which is used to introduce a specific null return of malloc in bg thread */
	FINC_RX_BUF_NULL = 29,

	/* The last guard item, don't put anything behind this one. */
	FINC_MAX_LIMITATION = 31,
} FAULT_INJECTION_TYPE;

#define FINC_HAS_FAULT(type) (gp_udpic_fault_inject_bitmap & (1U << (type)))

/*
 * testmode_check_interrupts
 * 		ML_CHECK_FOR_INTERRUPTS in test mode with interrupts injected.
 */
static inline void
testmode_check_interrupts(const char *caller_name, bool teardownActive)
{
	if (Gp_role != GP_ROLE_DISPATCH)
	{
		ML_CHECK_FOR_INTERRUPTS(teardownActive);
		return;
	}

	if (FINC_HAS_FAULT(FINC_INTR_QUERY_CANCEL) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		QueryCancelPending = true;
		InterruptPending = true;
	}
	else if (FINC_HAS_FAULT(FINC_INTR_PROC_DIE) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		ProcDiePending = true;
		InterruptPending = true;
	}

	ML_CHECK_FOR_INTERRUPTS(teardownActive);
}

/* We needs a private copy to corrupt the packet. */
#define FAULT_INJECT_BACKUP_PKT() \
do { \
	pktModified = true; \
	memcpy(&hdrbk, (void *) buffer, sizeof(icpkthdr)); \
} while (0)

/*
 * testmode_sendto
 * 		Many kinds of fault packets are injected in this function.
 */
static ssize_t
testmode_sendto(const char *caller_name, int socket, const void *buffer,
			   size_t length, int flags, const struct sockaddr *dest_addr,
			   socklen_t dest_len)
{
	int		n;
	int		testmode_length = length;
	int		icpkthdr_size = sizeof(icpkthdr);
	bool	is_pkt = false;
	bool	pktModified = false;
	int		fault_type;
	icpkthdr hdrbk;
	icpkthdr *msg = (icpkthdr *) buffer;

	if (!testmode_inject_fault(gp_udpic_fault_inject_percent))
		goto no_fault_inject;

	/*
	 * Generate a fault type.
	 */
	fault_type = random() % FINC_MAX_LIMITATION;

	/* Make sure we are modifying a packet. */
	if (length >= icpkthdr_size)
		is_pkt = true;

	/* Inject a fault */
	switch (fault_type)
	{
		case FINC_PKT_HEADER_SHORTEN:
			if (!FINC_HAS_FAULT(fault_type) || !is_pkt)
				break;
			testmode_length = icpkthdr_size - 1;
			write_log("inject fault to sendto: FINC_PKT_HEADER_SHORTEN");
			break;

		case FINC_PKT_PKT_SHORTEN:
			if (!FINC_HAS_FAULT(fault_type) || !is_pkt)
				break;
			if (length > icpkthdr_size)
				testmode_length--;
			write_log("inject fault to sendto: FINC_PKT_PKT_SHORTEN");
			break;

		case FINC_PKT_CRC_CORRUPT:
			if (!FINC_HAS_FAULT(fault_type) || !is_pkt)
				break;
			FAULT_INJECT_BACKUP_PKT();
			if (!gp_interconnect_full_crc)
				break;
			msg->crc++;
			write_log("inject fault to sendto: FINC_PKT_CRC_CORRUPT");
			break;

		case FINC_PKT_HEADER_LEN_ZERO:
			if (!FINC_HAS_FAULT(fault_type) || !is_pkt)
				break;
			FAULT_INJECT_BACKUP_PKT();
			msg->len = 0;
			write_log("inject fault to sendto: FINC_PKT_HEADER_LEN_ZERO");
			break;

		case FINC_PKT_HEADER_LEN_NEGATIVE:
			if (!FINC_HAS_FAULT(fault_type) || !is_pkt)
				break;
			FAULT_INJECT_BACKUP_PKT();
			msg->len = -1;
			write_log("inject fault to sendto: FINC_PKT_HEADER_LEN_NEGATIVE");
			break;

		case FINC_PKT_MISMATCH:
			if (!FINC_HAS_FAULT(fault_type) || !is_pkt)
				break;
			FAULT_INJECT_BACKUP_PKT();
			msg->srcPid = -1;	/* There is no such pid. */
			msg->icId = 0;
			msg->seq = 1;
			write_log("inject fault to sendto: FINC_PKT_MISMATCH");
			break;

		case FINC_OS_EAGAIN:
			if (!FINC_HAS_FAULT(fault_type))
				break;
			write_log("inject fault to sendto: FINC_OS_EAGAIN");
			errno = EAGAIN;
			return -1;

		case FINC_OS_EINTR:
			if (!FINC_HAS_FAULT(fault_type))
				break;
			write_log("inject fault to sendto: FINC_OS_EINTR");
			errno = EINTR;
			return -1;

		case FINC_NET_PKT_DUP:
			if (!FINC_HAS_FAULT(fault_type))
				break;
			write_log("inject fault to sendto: FINC_NET_PKT_DUP");
			if ((n = sendto(socket, buffer, testmode_length, flags, dest_addr, dest_len)) != length)
				return n;
			break;

		case FINC_OS_NET_INTERFACE:
			if (!FINC_HAS_FAULT(fault_type))
				break;
			write_log("inject fault to sendto: FINC_OS_NET_INTERFACE");
			errno = EFAULT;
			return -1;

		default:
			break;
	}

no_fault_inject:
	n = sendto(socket, buffer, testmode_length, flags, dest_addr, dest_len);

	if (pktModified)
		memcpy((void *) buffer, &hdrbk, sizeof(icpkthdr));
	return n;
}

/*
 * testmode_recvfrom
 * 		recvfrom function with faults injected.
 */
static ssize_t
testmode_recvfrom(const char *caller_name, int socket, void *restrict buffer,
				  size_t length, int flags, struct sockaddr *restrict address,
				  socklen_t *restrict address_len)
{
	int		fault_type;

	if (!testmode_inject_fault(gp_udpic_fault_inject_percent))
		goto no_fault_inject;

	fault_type = random() % FINC_MAX_LIMITATION;

	switch (fault_type)
	{
		case FINC_OS_EAGAIN:
			if (!FINC_HAS_FAULT(fault_type))
				break;
			write_log("inject fault to recvfrom: FINC_OS_EAGAIN");
			errno = EAGAIN;
			return -1;

		case FINC_OS_EINTR:
			if (!FINC_HAS_FAULT(fault_type))
				break;
			write_log("inject fault to recvfrom: FINC_OS_EINTR");
			errno = EINTR;
			return -1;

		case FINC_OS_EWOULDBLOCK:
			if (!FINC_HAS_FAULT(fault_type))
				break;
			write_log("inject fault to recvfrom: FINC_OS_EWOULDBLOCK");
			errno = EWOULDBLOCK;
			return -1;

		case FINC_NET_RECV_ZERO:
			if (!FINC_HAS_FAULT(fault_type))
				break;

			MemSet(buffer, 0, length);
			write_log("inject fault to recvfrom: FINC_NET_RECV_ZERO");
			return 0;

		case FINC_OS_NET_INTERFACE:
			if (!FINC_HAS_FAULT(fault_type))
				break;
			write_log("inject fault to recvfrom: FINC_OS_NET_INTERFACE");
			errno = EFAULT;
			return -1;

		default:
			break;
	}

no_fault_inject:
	return recvfrom(socket, buffer, length, flags, address, address_len);
}

/*
 * testmode_poll
 * 		poll function with faults injected.
 */
static int
testmode_poll(const char *caller_name, struct pollfd fds[], nfds_t nfds,
			  int timeout)
{
	int		fault_type;

	if (!testmode_inject_fault(gp_udpic_fault_inject_percent))
		goto no_fault_inject;

	fault_type = random() % FINC_MAX_LIMITATION;

	switch (fault_type)
	{
		case FINC_OS_EINTR:
			if (!FINC_HAS_FAULT(fault_type))
				break;
			write_log("inject fault to poll: FINC_OS_EINTR");
			errno = EINTR;
			return -1;

		case FINC_OS_NET_INTERFACE:
			if (!FINC_HAS_FAULT(fault_type))
				break;
			write_log("inject fault to poll: FINC_OS_NET_INTERFACE");
			errno = EFAULT;
			return -1;

		default:
			break;
	}

no_fault_inject:
	return poll(fds, nfds, timeout);
}

/*
 * testmode_socket
 * 		socket function with faults injected.
 *
 */
static int
testmode_socket(const char *caller_name, int domain, int type, int protocol)
{
	if (FINC_HAS_FAULT(FINC_OS_NET_INTERFACE) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		write_log("inject fault to socket: FINC_OS_NET_INTERFACE");
		errno = ENOMEM;
		return -1;
	}

	return socket(domain, type, protocol);
}

/*
 * testmode_bind
 *   	bind function with fault injected.
 *
 */
static int
testmode_bind(const char *caller_name, int socket,
			  const struct sockaddr *address, socklen_t address_len)
{
	if (FINC_HAS_FAULT(FINC_OS_NET_INTERFACE) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		write_log("inject fault to bind: FINC_OS_NET_INTERFACE");
		errno = EFAULT;
		return -1;
	}

	return bind(socket, address, address_len);
}

/*
 * testmode_getsockname
 * 		getsockname function with faults injected.
 *
 */
static int
testmode_getsockname(const char *caller_name, int socket,
					 struct sockaddr *restrict address,
					 socklen_t *restrict address_len)
{
	if (FINC_HAS_FAULT(FINC_OS_NET_INTERFACE) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		write_log("inject fault to getsockname: FINC_OS_NET_INTERFACE");
		errno = EFAULT;
		return -1;
	}

	return getsockname(socket, address, address_len);
}

/*
 * testmode_getsockopt
 * 		getsockopt function with faults injected
 */
static int
testmode_getsockopt(const char *caller_name, int socket, int level,
					int option_name, void *restrict option_value,
					socklen_t *restrict option_len)
{
	if (FINC_HAS_FAULT(FINC_OS_NET_INTERFACE) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		write_log("inject fault to getsockopt: FINC_OS_NET_INTERFACE");
		errno = EFAULT;
		return -1;
	}

	return getsockopt(socket, level, option_name, option_value, option_len);
}

/*
 * testmode_setsockopt
 * 		setsockopt with faults injected.
 */
static int
testmode_setsockopt(const char *caller_name, int socket, int level,
					int option_name, const void *option_value,
					socklen_t option_len)
{
	if (FINC_HAS_FAULT(FINC_OS_NET_INTERFACE) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		write_log("inject fault to setsockopt: FINC_OS_NET_INTERFACE");
		errno = ENOMEM;
		return -1;
	}

	return setsockopt(socket, level, option_name, option_value, option_len);
}

/*
 * testmode_pg_getaddrinfo_all
 * 		pg_getaddrinfo_all with faults injected.
 */
static int
testmode_pg_getaddrinfo_all(const char *caller_name, const char *hostname,
				   const char *servname, const struct addrinfo *hints,
				   struct addrinfo **res)
{
	if (FINC_HAS_FAULT(FINC_OS_NET_INTERFACE) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		write_log("inject fault to pg_getaddrinfo_all: FINC_OS_NET_INTERFACE");
		return -1;
	}

	return pg_getaddrinfo_all(hostname, servname, hints, res);
}

/*
 * testmode_malloc
 * 		malloc with faults injected.
 */
static void *
testmode_malloc(const char *caller_name, size_t size)
{
	void	*ret;

	if (FINC_HAS_FAULT(FINC_OS_MEM_INTERFACE) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		write_log("inject fault to malloc: FINC_OS_MEM_INTERFACE in %s()", caller_name);
		errno = ENOMEM;
		return NULL;
	}

	ret = malloc(size);

	pthread_mutex_lock(&icudp_malloc_tracking_lock);
	if (ret)
		icudp_malloc_times++;
	pthread_mutex_unlock(&icudp_malloc_tracking_lock);

	return ret;
}

/*
 * testmode_free
 * 		free function with free time tracking added.
 */
static void
testmode_free(const char *caller_name, void *ptr)
{
	if (ptr == NULL)
		return;

	pthread_mutex_lock(&icudp_malloc_tracking_lock);
	icudp_malloc_times--;
	pthread_mutex_unlock(&icudp_malloc_tracking_lock);
	free(ptr);
}

/*
 * testmode_palloc0
 * 		palloc0 with faults injected.
 */
static void *
testmode_palloc0(const char *caller_name, size_t size)
{
	if (FINC_HAS_FAULT(FINC_OS_MEM_INTERFACE) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		write_log("inject fault to palloc0: FINC_OS_MEM_INTERFACE");
		ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
				errmsg("inject fault to palloc0: FINC_OS_MEM_INTERFACE")));
	}

	return palloc0(size);
}

/*
 * testmode_pthread_create
 * 		pthread_create with faults injected.
 */
static int
testmode_pthread_create(const char *caller_name, pthread_t *thread,
		const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
	if (FINC_HAS_FAULT(FINC_OS_CREATE_THREAD) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
	{
		write_log("inject fault to pthread_create: FINC_OS_CREATE_THREAD");
		return ENOMEM;
	}

	return pthread_create(thread, attr, start_routine, arg);
}


#undef ML_CHECK_FOR_INTERRUPTS
#undef sendto
#undef recvfrom
#undef poll
#undef socket
#undef bind
#undef getsockname
#undef getsockopt
#undef setsockopt
#undef pg_getaddrinfo_all
#undef malloc
#undef free
#undef palloc0
#undef pthread_create

#define ML_CHECK_FOR_INTERRUPTS(teardownActive) \
	testmode_check_interrupts(PG_FUNCNAME_MACRO, teardownActive)

#define sendto(socket, buffer, length, flags, dest_addr, dest_len) \
	testmode_sendto(PG_FUNCNAME_MACRO, socket, buffer, length, flags, dest_addr, dest_len)

#define recvfrom(socket, buffer, length, flags, address, address_len) \
	testmode_recvfrom(PG_FUNCNAME_MACRO, socket, buffer, length, flags, address, address_len)

#define poll(fds, nfds, timeout) \
	testmode_poll(PG_FUNCNAME_MACRO, fds, nfds, timeout)

#define socket(domain, type, protocol) \
	testmode_socket(PG_FUNCNAME_MACRO, domain, type, protocol)

#define bind(socket, address, address_len) \
	testmode_bind(PG_FUNCNAME_MACRO, socket, address, address_len)

#define getsockname(socket, address, address_len) \
	testmode_getsockname(PG_FUNCNAME_MACRO, socket, address, address_len)

#define getsockopt(socket, level, option_name, option_value, option_len) \
	testmode_getsockopt(PG_FUNCNAME_MACRO, socket, level, option_name, option_value, option_len)

#define setsockopt(socket, level, option_name, option_value, option_len) \
	testmode_setsockopt(PG_FUNCNAME_MACRO, socket, level, option_name, option_value, option_len)

#define pg_getaddrinfo_all(hostname, servname, hints, res) \
	testmode_pg_getaddrinfo_all(PG_FUNCNAME_MACRO, hostname, servname, hints, res)

#define malloc(size) \
	testmode_malloc(PG_FUNCNAME_MACRO, size)

#define free(ptr) \
	testmode_free(PG_FUNCNAME_MACRO, ptr)

#define palloc0(size) \
	testmode_palloc0(PG_FUNCNAME_MACRO, size)

#define pthread_create(thread, attr, start_routine, arg) \
	testmode_pthread_create(PG_FUNCNAME_MACRO, thread, attr, start_routine, arg)
#endif

#endif
