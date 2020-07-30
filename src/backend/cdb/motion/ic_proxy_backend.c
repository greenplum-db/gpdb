/*-------------------------------------------------------------------------
 *
 * ic_proxy_backend.c
 *
 *    Interconnect Proxy Backend
 *
 * The functions in this file are all called by the backends.
 *
 * A backend and a client is the 2 end points of a domain socket, the backend
 * is in the QD/QE backend process, the client is in the proxy bgworker
 * process.
 *
 * TODO: at the moment the ic-tcp backend logic is reused by ic-proxy, in fact
 * the ic-proxy logic just lives inside ic_tcp.c, but in the future we may want
 * to build the ic-proxy logic independently, the potential benefits are:
 * - send / receive as ICProxyPkt directly: ic-tcp sends with a 4-byte header,
 *   in the proxy bgworker we need to unpack them and repack them into
 *   ICProxyPkt packets; the receiver needs to do it reversely; if we send /
 *   receive as ICProxyPkt directly we could reduce the parsing and memory
 *   copying;
 *
 * Note that the backend setup part of ic-proxy has been implemented by libuv
 * loop. Data flow send/recv and connection tear down is still based on ic-tcp.
 *
 * Copyright (c) 2020-Present Pivotal Software, Inc.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "cdb/ml_ipc.h"
#include "executor/execdesc.h"

#include "ic_proxy.h"
#include "ic_proxy_backend.h"
#include "ic_proxy_packet.h"
#include "ic_proxy_key.h"

#include <unistd.h>
#include <uv.h>

#define CONNECT_TIMER_TIMEOUT 100

typedef struct ICProxyBackend ICProxyBackend;

/*
 * ICProxyBackend represents a backend connection
 *
 * Connection is type of domain socket and each backend connection is identified by a
 * backend key.
 */
struct ICProxyBackend
{
	uv_pipe_t	pipe;		/* the libuv handle */

	ICProxyKey	key;		/* the key to identify a backend */

	/*
	 * TODO: MotionConn is used by ic_tcp, currently ic_proxy still reuses the code of
	 * ic_tcp to send/receive interconnect data. Keeping a MotionConn pointer here is
	 * to set pipe handle for ic_tcp, when connection between backend and proxy is
	 * established.
	 */
	MotionConn   *conn;		/* MotionConn used by ic_tcp */

	bool	isSender;		/* is motion sender */

	/* buffer to send/recv handshake messages */
	char	buffer[sizeof(ICProxyPkt)];

	/* 
	 * Messages are sent/received in an async way, so offset is used to point to the
	 * current position where the buffer send/recv next part of messages.
	 */
	int		offset;
	int		retryNum;		/* connect retry number */
};


static void ic_proxy_backend_walker(uv_handle_t *handle, void *arg);
static void ic_proxy_backend_schedule_connect(ICProxyBackend *backend);
static void ic_proxy_pkt_backend_alloc_buffer(uv_handle_t *handle, size_t suggested_size,
											  uv_buf_t *buf);
static ChunkTransportStateEntry *ic_proxy_backend_get_pentry(ICProxyBackend *backend);

/* uv callback */
static void ic_proxy_backend_on_interrupt_timer(uv_timer_t *timer);
static void ic_proxy_backend_on_cancel_from_qd_timer(uv_timer_t *timer);
static void ic_proxy_backend_on_connect_timer(uv_timer_t *timer);
static void ic_proxy_backend_on_connected(uv_connect_t *conn, int status);
static void ic_proxy_backend_on_sent_hello(uv_write_t *req, int status);
static void ic_proxy_backend_on_read_hello_ack(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
static void ic_proxy_backend_on_close(uv_handle_t *handle);

/*
 * Backend connection close callback
 */
static void
ic_proxy_backend_on_close(uv_handle_t *handle)
{
	ICProxyBackend *backend;
	ICProxyBackendContext * context;

	backend = CONTAINER_OF((void *) handle, ICProxyBackend, pipe);
	context = backend->pipe.loop->data;

	ic_proxy_log(LOG, "backend %s: backend connection closed, begin to reconnect.",
				 ic_proxy_key_to_str(&backend->key));

	/* 
	 * the previous pipe is already destroyed by uv_close, so we should
	 * re-init the domain socket pipe here before reconnect
	 */
	uv_pipe_init(&context->loop, &backend->pipe, false);
	ic_proxy_backend_schedule_connect(backend);
}

/*
 * Get ChunkTransportStateEntry based on send slice index of backend
 */
static ChunkTransportStateEntry *
ic_proxy_backend_get_pentry(ICProxyBackend *backend)
{
	ICProxyBackendContext *context;
	ChunkTransportState *pTransportStates;
	ChunkTransportStateEntry *pEntry = NULL;

	context = backend->pipe.loop->data;
	pTransportStates = (ChunkTransportState *)context->transportState;
	getChunkTransportState(pTransportStates, backend->key.sendSliceIndex,
						   &pEntry);
	return pEntry;
}

/*
 * Backend conenction receive HELLO ACK callback
 */
static void
ic_proxy_backend_on_read_hello_ack(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	ICProxyBackend *backend;
	const ICProxyPkt *pkt;
	int			ret;

	backend = CONTAINER_OF((void *) stream, ICProxyBackend, pipe);

	if (nread < 0)
	{
		/* UV_EOF is expected */
		if (nread == UV_EOF)
			ic_proxy_log(LOG, "backend %s: received EOF while receiving HELLO ACK.",
						 ic_proxy_key_to_str(&backend->key));
		else
			ic_proxy_log(WARNING, "backend %s: failed to recieve HELLO ACK: %s ",
						 ic_proxy_key_to_str(&backend->key), uv_strerror(nread));

		/* should close current pipe and try to reconnect */
		uv_close((uv_handle_t *) &backend->pipe, ic_proxy_backend_on_close);
		return;
	}
	/* Equivalent to EAGAIN or EWOULDBLOCK */
	else if (nread == 0)
		return;

	/* case: nread > 0 */
	Assert(nread <= sizeof(ICProxyPkt) - backend->offset);
	memcpy(backend->buffer + backend->offset, buf->base, nread);
	backend->offset += nread;

	if (backend->offset < sizeof(ICProxyPkt))
		return;

	/* we have received a complete HELLO ACK message */
	pkt = (ICProxyPkt *)backend->buffer;
	Assert(ic_proxy_pkt_is(pkt, IC_PROXY_MESSAGE_HELLO_ACK));
	uv_read_stop(stream);

	/* assign connection fd after domain socket successfully connected */
	ret = uv_fileno((uv_handle_t *)&backend->pipe, &backend->conn->sockfd);

	/* uv_fileno should not fail here */
	if (ret < 0)
		elog(ERROR, "backend %s: get connection fd failed: %s",
			 ic_proxy_key_to_str(&backend->key), uv_strerror(ret));

	/* ic_tcp compatitble code to modify ChunkTransportStateEntry for receiver */
	if (!backend->isSender)
	{
		ChunkTransportStateEntry *pEntry;

		pEntry = ic_proxy_backend_get_pentry(backend);

		MPP_FD_SET(backend->conn->sockfd, &pEntry->readSet);
		if (backend->conn->sockfd > pEntry->highReadSock)
			pEntry->highReadSock = backend->conn->sockfd;
	}
}

/*
 *  Allocate HELLO ACK buffer in the libuv style
 *
 *  Ensure the buf len plus offset is equal to the size of ICProxyPkt
 *  This could avoid receiving data after HELLO ACK message.
 */
static void
ic_proxy_pkt_backend_alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	ICProxyBackend *backend = (ICProxyBackend *) handle;

	buf->base = backend->buffer + backend->offset;
	buf->len = sizeof(ICProxyPkt) - backend->offset;
}

/*
 * Backend connected to a proxy.
 */
static void
ic_proxy_backend_on_sent_hello(uv_write_t *req, int status)
{
	ICProxyBackend *backend;

	backend = (ICProxyBackend *)req->handle;

	ic_proxy_free(req);

	if (status < 0)
	{
		ic_proxy_log(LOG, "backend %s: backend failed to send HELLO: %s",
					 ic_proxy_key_to_str(&backend->key), uv_strerror(status));
		uv_close((uv_handle_t *) &backend->pipe, ic_proxy_backend_on_close);
		return;
	}

	/* recieve hello ack */
	ic_proxy_log(LOG, "backend %s: backend connected, receiving HELLO ACK",
				 ic_proxy_key_to_str(&backend->key));

	backend->offset = 0;
	uv_read_start((uv_stream_t*)&backend->pipe, ic_proxy_pkt_backend_alloc_buffer,
				  ic_proxy_backend_on_read_hello_ack);
}

/*
 *  Put backend into connect queue and start connect timer
 */
static void
ic_proxy_backend_schedule_connect(ICProxyBackend *backend)
{
	ICProxyBackendContext *context;

	context = backend->pipe.loop->data;

	context->connectQueue = lappend(context->connectQueue, backend);

	/* 
	 * ref the connect timer, refer to connect timer in 
	 * ic_proxy_backend_init_context for detailed explanation.
	 */
	uv_ref((uv_handle_t *)&context->connectTimer);
}

/*
 * Connect callback
 *
 * Backend connected to a proxy, and begins to send HELLO message.
 */
static void
ic_proxy_backend_on_connected(uv_connect_t *conn, int status)
{
	ICProxyBackend *backend;
	uv_write_t *req;
	uv_buf_t	buf;
	ICProxyPkt *pkt;

	backend = (ICProxyBackend *)conn->handle;

	ic_proxy_free(conn);

	if (status < 0)
	{
		/* the proxy might just not get ready yet, retry later */
		ic_proxy_log(LOG, "backend %s: backend failed to connect: %s",
					 ic_proxy_key_to_str(&backend->key), uv_strerror(status));

		/* retry interval is 100ms and unit of interconnect_setup_timeout is second */
		if (interconnect_setup_timeout * 1000 <=  backend->retryNum * CONNECT_TIMER_TIMEOUT)
			ic_proxy_log(ERROR, "backend %s: Interconnect timeout: Unable to "
						 "complete setup of connection within time limit %d seconds",
						 ic_proxy_key_to_str(&backend->key), interconnect_setup_timeout);

		backend->retryNum++;

		/* should close current pipe and try to reconnect */
		uv_close((uv_handle_t *) &backend->pipe, ic_proxy_backend_on_close);
		return;
	}

	ic_proxy_log(LOG, "backend %s: backend connected, sending HELLO message.",
				 ic_proxy_key_to_str(&backend->key));

	/* 
	 * reuse backend buffer to avoid memory alloc, and thus it should not be freed
	 * in the write cb
	 */
	backend->offset = 0;
	pkt = (ICProxyPkt *)backend->buffer;

	ic_proxy_message_init(pkt, IC_PROXY_MESSAGE_HELLO, &backend->key);
	req = ic_proxy_new(uv_write_t);

	buf.base = (char *)pkt;
	buf.len = pkt->len;

	uv_write(req, (uv_stream_t *)&backend->pipe, &buf, 1, ic_proxy_backend_on_sent_hello);
}

/*
 * Timer callback: quick check_for_interrupt
 */
static void
ic_proxy_backend_on_interrupt_timer(uv_timer_t *timer)
{
	ChunkTransportState *pTransportStates;

	pTransportStates = uv_handle_get_data((uv_handle_t *)timer);
	ML_CHECK_FOR_INTERRUPTS(pTransportStates->teardownActive);
}

/*
 * Timer callback: slow checkForCancelFromQD
 */
static void
ic_proxy_backend_on_cancel_from_qd_timer(uv_timer_t *timer)
{
	ChunkTransportState *pTransportStates;

	pTransportStates = uv_handle_get_data((uv_handle_t *)timer);
	checkForCancelFromQD(pTransportStates);
}

/*
 * Timer callback
 *
 * Reconnect all the pending requests in connect queue
 */
static void
ic_proxy_backend_on_connect_timer(uv_timer_t *timer)
{
	ICProxyBackendContext	   *context;
	uv_connect_t   *req;
	ListCell	   *cell;
	struct			sockaddr_un addr;

	context = timer->loop->data;
	foreach (cell, context->connectQueue)
	{
		ICProxyBackend *backend = (ICProxyBackend *)lfirst(cell);

		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		ic_proxy_build_server_sock_path(addr.sun_path, sizeof(addr.sun_path));

		req = ic_proxy_new(uv_connect_t);

		uv_pipe_connect(req, &backend->pipe, addr.sun_path,
						ic_proxy_backend_on_connected);
	}

	/* 
	 * unref the connect timer, refer to connect timer in 
	 * ic_proxy_backend_init_context for detailed explanation.
	 */
	uv_unref((uv_handle_t *)&context->connectTimer);

	list_free(context->connectQueue);
	context->connectQueue = NIL;
}

/*
 * Using walker to close the pipe handle
 *
 * The pipe handles are only maintained by libuv, we could only use uv_walker
 * to access and close these pipe.
 */
static void
ic_proxy_backend_walker(uv_handle_t *handle, void *arg)
{
	switch (handle->type)
	{
		case UV_NAMED_PIPE:
			/* 
			 * Passing NULL instead of ic_proxy_backend_on_close as the callback,
			 * since ic_proxy_backend_on_close will schedule a reconnection, but we
			 * just want to close the pipe handle here.
			 */
			uv_close(handle, NULL);
			break;
		default:
			break;
	}
}

/*
 * Putting the connection request into the connect queue and waiting for connect
 * timer to process the connect event when uv_loop is running
 */
void
ic_proxy_backend_connect(ICProxyBackendContext *context, ChunkTransportStateEntry *pEntry, MotionConn *conn, bool isSender)
{
	ICProxyBackend *backend;

	backend = ic_proxy_new(ICProxyBackend);

	backend->isSender = isSender;

	/* TODO: remove conn after we decouple ic_proxy and ic_tcp */
	backend->conn = conn;
	backend->retryNum = 0;
	
	/* message key for a HELLO message */
	ic_proxy_key_init(&backend->key,					/* key itself */
					  gp_session_id,					/* sessionId */
					  gp_command_count,					/* commandId */
					  pEntry->sendSlice->sliceIndex,	/* sendSliceIndex */
					  pEntry->recvSlice->sliceIndex,	/* recvSliceIndex */
					  GpIdentity.segindex,				/* localContentId */
					  GpIdentity.dbid,					/* localDbid */
					  MyProcPid,						/* localPid */
					  conn->cdbProc->contentid,			/* remoteContentId */
					  conn->cdbProc->dbid,				/* remoteDbid */
					  conn->cdbProc->pid);				/* remotePid */


	uv_pipe_init(&context->loop, &backend->pipe, false);

	/* put connection into queue */
	ic_proxy_backend_schedule_connect(backend);
}

/*
 * Initialize the icproxy backend context
 */
void
ic_proxy_backend_init_context(ChunkTransportState *state)
{
	ICProxyBackendContext *context;
	
	/* initialize backend context */
	state->proxyContext = palloc0(sizeof(ICProxyBackendContext));
	state->proxyContext->transportState = state;

	context = state->proxyContext;

	/* init libuv loop */
	uv_loop_init(&context->loop);
	uv_loop_set_data(&context->loop, (void *)context);

	/* interrupt timer */
	uv_timer_init(&context->loop, &context->interruptTimer);
	uv_handle_set_data((uv_handle_t *)&context->interruptTimer, (void *)context->transportState);
	uv_timer_start(&context->interruptTimer, ic_proxy_backend_on_interrupt_timer, 0, 500);
	uv_unref((uv_handle_t *)&context->interruptTimer);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		uv_timer_init(&context->loop, &context->cancelFromQDTimer);
		uv_handle_set_data((uv_handle_t *)&context->cancelFromQDTimer, (void *)context->transportState);
		uv_timer_start(&context->cancelFromQDTimer, ic_proxy_backend_on_cancel_from_qd_timer, 0, 2000);
		uv_unref((uv_handle_t *)&context->cancelFromQDTimer);
	}

	/* 
	 * Connect timer is used to process the connect/reconnect request in
	 * conenct queue periodically. At the beginng of uv_run, all the
	 * cached connect request in queue will be processed immediately.
	 * If connect failed, the reconnect request will be push back to the
	 * queue, and wait for the connect timer to trigger the reconnect.
	 *
	 * Libuv uses ref/unref to mark whether the event if OK to exit,
	 * the uv_run will stop if all the events are in state of unref.
	 *
	 * When a request is pushed into the queue, we need to call uv_ref
	 * to pin the connect timer, and after we processed all the existing
	 * request in the queue, we should call uv_unref to unpin the timer
	 * to tell uv_loop we are OK to exit.
	 *
	 * Note that ref/unref in libuv is not a reference counting, so call
	 * ref/unref repeatly is safe.
	 */
	uv_timer_init(&context->loop, &context->connectTimer);
	uv_timer_start(&context->connectTimer, ic_proxy_backend_on_connect_timer, 0, CONNECT_TIMER_TIMEOUT);
	uv_unref((uv_handle_t *)&context->connectTimer);
}

/*
 * Close the icproxy backend context
 */
void
ic_proxy_backend_close_context(ChunkTransportState *state)
{
	ICProxyBackendContext *context;
	int ret;

	if (!state->proxyContext)
		return;

	context = state->proxyContext;

	/* close interrupt timer and connect timer */
	if (Gp_role == GP_ROLE_DISPATCH)
		uv_close((uv_handle_t *)&context->cancelFromQDTimer, NULL);
	uv_close((uv_handle_t *)&context->interruptTimer, NULL);
	uv_close((uv_handle_t *)&context->connectTimer, NULL);

	/* 
	 * Use walker to close the pipe handle.
	 *
	 * Since we reuse ic-tcp code to handle data flow between backend and proxy,
	 * we have to transfer the owner of pipe fd to MotionConn. The fd is closed
	 * by ic-tcp code, but the pipe handle in libuv loop should be closed by
	 * ourselves before we close the loop. This is tricky and should be fixed
	 * when we retire the ic-tcp code and use libuv to handle data flow between
	 * backend and proxy.
	 */
	uv_walk(&context->loop, ic_proxy_backend_walker, NULL);
	uv_run(&context->loop, UV_RUN_DEFAULT);

	ret = uv_loop_close(&context->loop);

	if (ret == UV_EBUSY )
		elog(WARNING,"ic-proxy-backend: ic_proxy_backend_close_loop failed: %s", uv_strerror(ret));

	pfree(state->proxyContext);
	state->proxyContext = NULL;
}

/*
 * Run the libuv main loop
 */
void ic_proxy_backend_run_loop(ICProxyBackendContext *context)
{
	uv_run(&context->loop, UV_RUN_DEFAULT);
}
