/*-------------------------------------------------------------------------
 *
 * ic_proxy_main.c
 *
 *	  The main loop of the ic-proxy, it listens for both new peers and new
 *	  clients, it also establish the peer connections.
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "utils/guc.h"
#include "utils/memutils.h"

#include "ic_proxy_server.h"
#include "ic_proxy_addr.h"
#include "ic_proxy_pkt_cache.h"

#include <uv.h>

#include <unistd.h>

static uv_loop_t	ic_proxy_server_loop;
static uv_signal_t	ic_proxy_server_signal_hup;
static uv_signal_t	ic_proxy_server_signal_int;
static uv_signal_t	ic_proxy_server_signal_term;
static uv_signal_t	ic_proxy_server_signal_stop;
static uv_timer_t	ic_proxy_server_timer;

static uv_tcp_t		ic_proxy_peer_listener;
static bool			ic_proxy_peer_listening;

static uv_pipe_t	ic_proxy_client_listener;
static bool			ic_proxy_client_listening;

static int			ic_proxy_server_exit_code = 1;

/* pipe to check whether postmaster is alive */
static uv_pipe_t	ic_proxy_postmaster_pipe;

/*
 * The peer listener is closed.
 */
static void
ic_proxy_server_peer_listener_on_closed(uv_handle_t *handle)
{
	ic_proxy_log(LOG, "ic-proxy-server: peer listener: closed");

	/* A new peer listener will be created on the next timer callback */
	ic_proxy_peer_listening = false;
}

/*
 * New peer arrives.
 */
static void
ic_proxy_server_on_new_peer(uv_stream_t *server, int status)
{
	ICProxyPeer *peer;
	int			ret;

	if (status < 0)
	{
		ic_proxy_log(WARNING, "ic-proxy-server: new peer error: %s",
					 uv_strerror(status));

		uv_close((uv_handle_t *) server,
				 ic_proxy_server_peer_listener_on_closed);
		return;
	}

	ic_proxy_log(LOG, "ic-proxy-server: new peer to the server");

	peer = ic_proxy_peer_new(server->loop,
							 IC_PROXY_INVALID_CONTENT, IC_PROXY_INVALID_DBID);

	ret = uv_accept(server, (uv_stream_t *) &peer->tcp);
	if (ret < 0)
	{
		ic_proxy_log(WARNING, "ic-proxy-server: fail to accept new peer: %s",
					 uv_strerror(ret));
		ic_proxy_peer_free(peer);
		return;
	}

	/* TODO: it is better to only touch the states in peer.c */
	peer->state |= IC_PROXY_PEER_STATE_ACCEPTED;

	/* Dump some connection information, not very useful though */
	{
		struct sockaddr_storage peeraddr;
		int			addrlen = sizeof(peeraddr);
		char		name[HOST_NAME_MAX];

		uv_tcp_getpeername(&peer->tcp, (struct sockaddr *) &peeraddr, &addrlen);
		if (peeraddr.ss_family == AF_INET)
		{
			struct sockaddr_in *peeraddr4 = (struct sockaddr_in *) &peeraddr;

			uv_ip4_name(peeraddr4, name, sizeof(name));

			ic_proxy_log(LOG, "ic-proxy-server: the new peer is from %s:%d",
						 name, ntohs(peeraddr4->sin_port));
		}
		else if (peeraddr.ss_family == AF_INET6)
		{
			struct sockaddr_in6 *peeraddr6 = (struct sockaddr_in6 *) &peeraddr;

			uv_ip6_name(peeraddr6, name, sizeof(name));

			ic_proxy_log(LOG, "ic-proxy-server: the new peer is from %s:%d",
						 name, ntohs(peeraddr6->sin6_port));
		}
	}

	ic_proxy_peer_read_hello(peer);
}

/*
 * Setup the peer listener.
 *
 * The peer listener listens on a tcp socket, the peer connections will come
 * through it.
 */
static void
ic_proxy_server_peer_listener_init(uv_loop_t *loop)
{
	const ICProxyAddr *addr;
	uv_tcp_t   *listener = &ic_proxy_peer_listener;
	int			fd = -1;
	int			ret;

	if (ic_proxy_addrs == NIL)
		return;

	if (ic_proxy_peer_listening)
		return;

	/* Get the addr from the gp_interconnect_proxy_addresses */
	addr = ic_proxy_get_my_addr();
	if (addr == NULL)
		/* Cannot get my addr, maybe the setting is invalid */
		return;

#if IC_PROXY_LOG_LEVEL <= LOG
	{
		char		name[HOST_NAME_MAX] = "unknown";
		int			port = 0;
		int			family;
		int			ret;

		ret = ic_proxy_extract_addr((struct sockaddr *) &addr->addr,
									name, sizeof(name), &port, &family);
		if (ret == 0)
			ic_proxy_log(LOG,
						 "ic-proxy-server: setting up peer listener on %s:%s (%s:%d family=%d)",
						 addr->hostname, addr->service, name, port, family);
		else
			ic_proxy_log(WARNING,
						 "ic-proxy-server: setting up peer listener on %s:%s (%s:%d family=%d) (fail to extract the address: %s)",
						 addr->hostname, addr->service, name, port, family,
						 uv_strerror(ret));
	}
#endif /* IC_PROXY_LOG_LEVEL <= LOG */

	/*
	 * It is important to set TCP_NODELAY, otherwise we will suffer from
	 * significant latency and get very bad OLTP performance.
	 */
	uv_tcp_init(loop, listener);
	uv_tcp_nodelay(listener, true);

	ret = uv_tcp_bind(listener, (struct sockaddr *) &addr->sockaddr, 0);
	if (ret < 0)
	{
		ic_proxy_log(WARNING, "ic-proxy-server: tcp: fail to bind: %s",
					 uv_strerror(ret));
		return;
	}

	ret = uv_listen((uv_stream_t *) listener,
					IC_PROXY_BACKLOG, ic_proxy_server_on_new_peer);
	if (ret < 0)
	{
		ic_proxy_log(WARNING, "ic-proxy-server: tcp: fail to listen: %s",
					 uv_strerror(ret));
		return;
	}

	uv_fileno((uv_handle_t *) listener, &fd);
	ic_proxy_log(LOG, "ic-proxy-server: tcp: listening on socket %d", fd);

	ic_proxy_peer_listening = true;
}

/*
 * The client listener is closed.
 */
static void
ic_proxy_server_client_listener_on_closed(uv_handle_t *handle)
{
	ic_proxy_log(LOG, "ic-proxy-server: client listener: closed");

	/* A new client listener will be created on the next timer callback */
	ic_proxy_client_listening = false;
}

/*
 * New client arrives.
 */
static void
ic_proxy_server_on_new_client(uv_stream_t *server, int status)
{
	ICProxyClient *client;
	int			ret;

	if (status < 0)
	{
		ic_proxy_log(WARNING, "ic-proxy-server: new client error: %s",
					 uv_strerror(status));

		uv_close((uv_handle_t *) server,
				 ic_proxy_server_client_listener_on_closed);
		return;
	}

	ic_proxy_log(LOG, "ic-proxy-server: new client to the server");

	client = ic_proxy_client_new(server->loop, false);

	ret = uv_accept(server, ic_proxy_client_get_stream(client));
	if (ret < 0)
	{
		ic_proxy_log(WARNING, "ic-proxy-server: fail to accept new client: %s",
					 uv_strerror(ret));
		return;
	}

	ic_proxy_client_read_hello(client);
}

/*
 * Setup the client listener.
 *
 * The client listener listens on a domain socket, the client connections will
 * come through it.
 */
static void
ic_proxy_server_client_listener_init(uv_loop_t *loop)
{
	uv_pipe_t  *listener = &ic_proxy_client_listener;
	char		path[MAXPGPATH];
	int			fd = -1;
	int			ret;

	if (ic_proxy_client_listening)
		return;

	ic_proxy_build_server_sock_path(path, sizeof(path));

	/* FIXME: do not unlink here */
	ic_proxy_log(LOG, "unlink(%s) ...", path);
	unlink(path);

	ic_proxy_log(LOG, "ic-proxy-server: setting up client listener on address %s",
				 path);

	ret = uv_pipe_init(loop, listener, false);
	if (ret < 0)
	{
		ic_proxy_log(WARNING,
					 "ic-proxy-server: fail to init a client listener: %s",
					 uv_strerror(ret));
		return;
	}

	ret = uv_pipe_bind(listener, path);
	if (ret < 0)
	{
		ic_proxy_log(WARNING, "ic-proxy-server: pipe: fail to bind(%s): %s",
					 path, uv_strerror(ret));
		return;
	}

	ret = uv_listen((uv_stream_t *) listener,
					IC_PROXY_BACKLOG, ic_proxy_server_on_new_client);
	if (ret < 0)
	{
		ic_proxy_log(WARNING, "ic-proxy-server: pipe: fail to listen on path %s: %s",
					 path, uv_strerror(ret));
		return;
	}

	uv_fileno((uv_handle_t *) listener, &fd);
	ic_proxy_log(LOG, "ic-proxy-server: pipe: listening on socket %d", fd);

	/*
	 * Dump the inode of the domain socket file, this helps us to know that the
	 * file is replaced by someone.  This is not likely to happen, we have
	 * carefully choosen the file path to not conflict with each other.
	 */
	{
		struct stat	st;

		stat(path, &st);
		ic_proxy_log(LOG, "ic-proxy-server: dev=%lu, inode=%lu, path=%s",
					 st.st_dev, st.st_ino, path);
	}

	ic_proxy_client_listening = true;
}

/*
 * Establish the peer connections.
 *
 * A proxy connects to all the other proxies, all these connections form the
 * proxy network.  Only one connection is needed between 2 proxies, this is
 * ensured by a policy that "proxy X connects to proxy Y iff X > Y".  To support
 * mirror promotion, X attempts to connect to Y even if Y is a mirror, or even
 * if we have connected to Y's primary.  In fact we do not know whether Y is a
 * mirror or not, and we do not care.
 */
static void
ic_proxy_server_ensure_peers(uv_loop_t *loop)
{
	ListCell   *cell;

	foreach(cell, ic_proxy_addrs)
	{
		ICProxyAddr *addr = lfirst(cell);
		ICProxyPeer *peer;

		if (addr->content >= GpIdentity.segindex)
			continue;
		if (addr->dbid == GpIdentity.dbid)
			continue; /* do not connect to my primary / mirror */

		/*
		 * First get the peer with the peer id, then connect to it.  The peer
		 * can be a placeholder, can be in the progress of a connection, or can
		 * be connected, the ic_proxy_peer_connect() function will take care of
		 * the state.
		 */
		peer = ic_proxy_peer_blessed_lookup(loop, addr->content, addr->dbid);
		ic_proxy_peer_connect(peer, (struct sockaddr_in *) addr);
	}
}

/*
 * Timer handler.
 *
 * This is used to maintain the proxy-proxy network, as well as the client and
 * peer listeners.
 */
static void
ic_proxy_server_on_timer(uv_timer_t *timer)
{
	ic_proxy_server_peer_listener_init(timer->loop);
	ic_proxy_server_ensure_peers(timer->loop);
	ic_proxy_server_client_listener_init(timer->loop);
}

/*
 * Signal handler.
 *
 * Signals are handled via the signalfd() call in libuv, so this is a normal
 * callback as others, nothing special, errors can be raised, too.
 */
static void
ic_proxy_server_on_signal(uv_signal_t *handle, int signum)
{
	ic_proxy_log(WARNING, "ic-proxy-server: received signal %d", signum);

	if (signum == SIGHUP)
	{
		ProcessConfigFile(PGC_SIGHUP);

		ic_proxy_reload_addresses(handle->loop);

		ic_proxy_server_peer_listener_init(handle->loop);
		ic_proxy_server_ensure_peers(handle->loop);
		ic_proxy_server_client_listener_init(handle->loop);
	}
	else
	{
		uv_stop(handle->loop);
	}
}

/*
 * callback when received data from ic_proxy_postmaster_pipe
 */
static void
ic_proxy_server_on_read_postmaster_pipe(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	/* return the pkt to cache freelist, we don't care about the buffer content */
	if (buf->base)
		ic_proxy_pkt_cache_free(buf->base);

	/* nread = 0 means EAGAIN and EWOULDBLOCK, while nread = EOF means postmaster is dead */
	if (nread == UV_EOF)
		proc_exit(1);
	else if (nread < 0)
		ic_proxy_log(FATAL, "read on postmaster death monitoring pipe failed: %s", uv_strerror(nread));
	else if (nread > 0)
		ic_proxy_log(FATAL, "unexpected data in postmaster death monitoring pipe with length: %ld", nread);
}

/*
 * The main loop of the ic-proxy.
 */
int
ic_proxy_server_main(void)
{
	char		path[MAXPGPATH];

	ic_proxy_log(LOG, "ic-proxy-server: setting up");

	ic_proxy_pkt_cache_init(IC_PROXY_MAX_PKT_SIZE);

	uv_loop_init(&ic_proxy_server_loop);

	ic_proxy_reload_addresses(&ic_proxy_server_loop);

	ic_proxy_router_init(&ic_proxy_server_loop);
	ic_proxy_peer_table_init();
	ic_proxy_client_table_init();

	ic_proxy_peer_listening = false;
	ic_proxy_client_listening = false;

	uv_signal_init(&ic_proxy_server_loop, &ic_proxy_server_signal_hup);
	uv_signal_start(&ic_proxy_server_signal_hup, ic_proxy_server_on_signal, SIGHUP);

	uv_signal_init(&ic_proxy_server_loop, &ic_proxy_server_signal_int);
	uv_signal_start(&ic_proxy_server_signal_int, ic_proxy_server_on_signal, SIGINT);

	/* on master */
	uv_signal_init(&ic_proxy_server_loop, &ic_proxy_server_signal_term);
	uv_signal_start(&ic_proxy_server_signal_term, ic_proxy_server_on_signal, SIGTERM);

	/* on segments */
	uv_signal_init(&ic_proxy_server_loop, &ic_proxy_server_signal_stop);
	uv_signal_start(&ic_proxy_server_signal_stop, ic_proxy_server_on_signal, SIGQUIT);

	/* TODO: we could stop the timer if all the peers are connected */
	uv_timer_init(&ic_proxy_server_loop, &ic_proxy_server_timer);
	uv_timer_start(&ic_proxy_server_timer, ic_proxy_server_on_timer, 100, 1000);

	/* monitor the postmaster pipe to check whether postmaster is still alive */
	uv_pipe_init(&ic_proxy_server_loop, &ic_proxy_postmaster_pipe, false);
	uv_pipe_open(&ic_proxy_postmaster_pipe, postmaster_alive_fds[POSTMASTER_FD_WATCH]);
	uv_read_start((uv_stream_t *)&ic_proxy_postmaster_pipe, ic_proxy_pkt_cache_alloc_buffer,
				  ic_proxy_server_on_read_postmaster_pipe);

	ic_proxy_log(LOG, "ic-proxy-server: running");

	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();

	/*
	 * return non-zero value so we are restarted by the postmaster, but this
	 * behavior can be controled by calling ic_proxy_server_quit()
	 */
	ic_proxy_server_exit_code = 1;
	uv_run(&ic_proxy_server_loop, UV_RUN_DEFAULT);
	uv_loop_close(&ic_proxy_server_loop);

	ic_proxy_log(LOG, "ic-proxy-server: closing");

	ic_proxy_client_table_uninit();
	ic_proxy_peer_table_uninit();
	ic_proxy_router_uninit();

	ic_proxy_build_server_sock_path(path, sizeof(path));
#if 0
	ic_proxy_log(LOG, "unlink(%s) ...", path);
	unlink(path);
#endif

	ic_proxy_pkt_cache_uninit();

	ic_proxy_log(LOG, "ic-proxy-server: closed with code %d",
				 ic_proxy_server_exit_code);

	return ic_proxy_server_exit_code;
}

void
ic_proxy_server_quit(uv_loop_t *loop, bool relaunch)
{
	ic_proxy_log(LOG, "ic-proxy-server: quiting");

	if (relaunch)
		/* return non-zero value so we are restarted by the postmaster */
		ic_proxy_server_exit_code = 1;
	else
		ic_proxy_server_exit_code = 0;

	/*
	 * we can't close the loop directly, we need to properly shutdown all the
	 * clients first.
	 */
	if (ic_proxy_peer_listening)
	{
		uv_unref((uv_handle_t *) &ic_proxy_peer_listener);
		uv_close((uv_handle_t *) &ic_proxy_peer_listener, NULL);
	}
	if (ic_proxy_client_listening)
	{
		uv_unref((uv_handle_t *) &ic_proxy_client_listener);
		uv_close((uv_handle_t *) &ic_proxy_client_listener, NULL);
	}
	uv_timer_stop(&ic_proxy_server_timer);
	uv_unref((uv_handle_t *) &ic_proxy_server_signal_hup);
	uv_unref((uv_handle_t *) &ic_proxy_server_signal_term);
	uv_unref((uv_handle_t *) &ic_proxy_server_signal_stop);

#if 0
	uv_client_table_disconnect_all();
#endif

	/*
	 * do not close the loop directly, it will quit automatically after all the
	 * clients are closed.
	 */
#if 0
	uv_loop_close(loop);
#endif
}
