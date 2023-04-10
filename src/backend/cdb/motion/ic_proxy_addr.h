/*-------------------------------------------------------------------------
 *
 * ic_proxy_addr.h
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef IC_PROXY_ADDR_H
#define IC_PROXY_ADDR_H

/*
 * Some systems like FreeBSD or MacOS do not define HOST_NAME_MAX. In general,
 * 255 seems to be a safe fallback value and is POSIX.
 */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

typedef struct ICProxyAddr ICProxyAddr;


struct ICProxyAddr
{
	struct sockaddr_storage sockaddr;

	int			dbid;
	int			content;

	/*
	 * Below two attributes are arguments to uv_getaddrinfo().
	 *
	 * That API allows "service" to be either a port number or a service name,
	 * like "http".  In our case each segment needs a unique port on its host,
	 * so it is more convenient to specify port numbers directly, so we only
	 * support the port numbers in gp_interconnect_proxy_addresses, service
	 * names will be considered as syntax errors.
	 */
	char		hostname[HOST_NAME_MAX];	/* hostname or IP */
	char		service[32];				/* port number as a string */

	uv_getaddrinfo_t req;
};


/*
 * List<ICProxyAddr *>
 */
extern List		   *ic_proxy_addrs;
extern List		   *ic_proxy_prev_addrs;

/*
 * List<unowned ICProxyAddr *>
 */
extern List		   *ic_proxy_removed_addrs;
extern List		   *ic_proxy_added_addrs;


extern void ic_proxy_reload_addresses(uv_loop_t *loop);
extern const ICProxyAddr *ic_proxy_get_my_addr(void);
extern int ic_proxy_addr_get_port(const ICProxyAddr *addr);
extern int ic_proxy_extract_sockaddr(const struct sockaddr *addr,
									 char *name, size_t namelen,
									 int *port, int *family);


#endif   /* IC_PROXY_ADDR_H */
