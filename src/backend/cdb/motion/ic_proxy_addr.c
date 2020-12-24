/*-------------------------------------------------------------------------
 *
 * ic_proxy_addr.c
 *
 *    Interconnect Proxy Addresses
 *
 * Maintain the address information of all the proxies, which is set by the GUC
 * gp_interconnect_proxy_addresses.
 *
 * FIXME: currently that GUC can not be reloaded with "gpstop -u", so we must
 * restart the cluster to update the setting.  This causes problems during
 * online expansion, when new segments are added to the cluster, we must update
 * this GUC to include their information, so until the cluster is restarted all
 * the ic-proxy mode queries will hang.
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */


#include <uv.h>

#include "ic_proxy.h"
#include "ic_proxy_addr.h"


/*
 * List<ICProxyAddr *>, the current addresses list.
 */
List	   *ic_proxy_addrs = NIL;

/*
 * List<ICProxyAddr *>, the previous addresses list.
 *
 * It holds the memory of all the addresses, so the classified lists can only
 * hold a ref.
 */
List	   *ic_proxy_prev_addrs = NIL;

/*
 * List<unowned ICProxyAddr *>, the classified addresses lists.
 *
 * - if an address is removed from the GUC gp_interconnect_proxy_addresses,
 *   it is put in the "removed" list;
 * - if an address is newly added to the GUC, it is in the "added" list;
 * - if an address is updated, it is in both the "removed" and "added" lists;
 *
 * The addresses of these lists must not be freed, they are actually held by
 * ic_proxy_addrs or ic_proxy_prev_addrs.
 */
List	   *ic_proxy_removed_addrs = NIL;
List	   *ic_proxy_added_addrs = NIL;

/*
 * List<ICProxyAddr *>, the addresses list that are being resolved.
 */
static List *ic_proxy_unknown_addrs = NIL;

/*
 * My address.
 */
static ICProxyAddr *ic_proxy_my_addr = NULL;

static int
ic_proxy_addr_compare_dbid(const void *a, const void *b, void *arg)
{
	const ICProxyAddr *addr1 = a;
	const ICProxyAddr *addr2 = b;

	return addr1->dbid - addr2->dbid;
}

/*
 * Classify the addrs as added, deleted, updated and unchanged.
 *
 * Both the old and new lists must be sorted by dbid, the caller is responsible
 * to ensure this.
 */
static void
ic_proxy_classify_addresses(List *oldaddrs, List *newaddrs)
{
	ListCell   *lcold;
	ListCell   *lcnew;

	ic_proxy_added_addrs = ic_proxy_list_free(ic_proxy_added_addrs);
	ic_proxy_removed_addrs = ic_proxy_list_free(ic_proxy_removed_addrs);

	lcold = list_head(oldaddrs);
	lcnew = list_head(newaddrs);
	while (lcold && lcnew)
	{
		ICProxyAddr *old = lfirst(lcold);
		ICProxyAddr *new = lfirst(lcnew);

		if (old->dbid < new->dbid)
		{
			/* the address is removed */
			ic_proxy_removed_addrs = lappend(ic_proxy_removed_addrs, old);
			lcold = lnext(lcold);
		}
		else if (old->dbid > new->dbid)
		{
			/* the address is newly added */
			ic_proxy_added_addrs = lappend(ic_proxy_added_addrs, new);
			lcnew = lnext(lcnew);
		}
		/*
		 * note that the new->sockaddr is not filled yet, so we must compare
		 * with the hostname and service as strings.
		 */
		else if (strcmp(old->service, new->service) ||
				 strcmp(old->hostname, new->hostname))
		{
			/* the address is updated */
			ic_proxy_removed_addrs = lappend(ic_proxy_removed_addrs, old);
			ic_proxy_added_addrs = lappend(ic_proxy_added_addrs, new);
			lcold = lnext(lcold);
			lcnew = lnext(lcnew);
		}
		else
		{
			/* the address is unchanged */
			lcold = lnext(lcold);
			lcnew = lnext(lcnew);
		}
	}

	/* all the addresses remaining in the old list are removed */
	for ( ; lcold; lcold = lnext(lcold))
	{
		ICProxyAddr *old = lfirst(lcold);

		ic_proxy_removed_addrs = lappend(ic_proxy_removed_addrs, old);
	}

	/* all the addresses remaining in the new list are newly added */
	for ( ; lcnew; lcnew = lnext(lcnew))
	{
		ICProxyAddr *new = lfirst(lcnew);

		ic_proxy_added_addrs = lappend(ic_proxy_added_addrs, new);
	}
}

/*
 * Resolved one address.
 */
static void
ic_proxy_addr_on_getaddrinfo(uv_getaddrinfo_t *req,
							 int status, struct addrinfo *res)
{
	ICProxyAddr *addr = CONTAINER_OF((void *) req, ICProxyAddr, req);

	ic_proxy_unknown_addrs = list_delete_ptr(ic_proxy_unknown_addrs, addr);

	if (status != 0)
	{
		if (status == UV_ECANCELED)
		{
			/* the req is cancelled, nothing to do */
		}
		else
			ic_proxy_log(WARNING,
						 "ic-proxy-addr: seg%d,dbid%d: fail to resolve the hostname \"%s\":%s: %s",
						 addr->content, addr->dbid,
						 addr->hostname, addr->service,
						 uv_strerror(status));

		ic_proxy_free(addr);
	}
	else
	{
		struct addrinfo *iter;

		/* should we follow the logic in getDnsCachedAddress() ? */
		for (iter = res; iter; iter = iter->ai_next)
		{
			if (iter->ai_family == AF_UNIX)
				continue;

#if IC_PROXY_LOG_LEVEL <= LOG
			{
				char		name[HOST_NAME_MAX] = "unknown";
				int			port = 0;
				int			family;
				int			ret;

				ret = ic_proxy_extract_addr(iter->ai_addr, name, sizeof(name),
											&port, &family);
				if (ret == 0)
					ic_proxy_log(LOG,
								 "ic-proxy-addr: seg%d,dbid%d: resolved address %s:%s -> %s:%d family=%d",
								 addr->content, addr->dbid,
								 addr->hostname, addr->service,
								 name, port, family);
				else
					ic_proxy_log(LOG,
								 "ic-proxy-addr: seg%d,dbid%d: resolved address %s:%s -> %s:%d family=%d (fail to extract the address: %s)",
								 addr->content, addr->dbid,
								 addr->hostname, addr->service,
								 name, port, family,
								 uv_strerror(ret));
			}
#endif /* IC_PROXY_LOG_LEVEL <= LOG */

			memcpy(&addr->sockaddr, iter->ai_addr, iter->ai_addrlen);
			ic_proxy_addrs = lappend(ic_proxy_addrs, addr);
			break;
		}
	}

	if (res)
		uv_freeaddrinfo(res);
}

/*
 * Reload the addresses from the GUC gp_interconnect_proxy_addresses.
 *
 * The caller is responsible to load the up-to-date setting of that GUC by
 * calling ProcessConfigFile().
 */
void
ic_proxy_reload_addresses(uv_loop_t *loop)
{
	/*
	 * save the old addresses to the "prev" list, it is used to know the diffs
	 * of the addresses.
	 */
	ic_proxy_prev_addrs = ic_proxy_list_free_deep(ic_proxy_prev_addrs);
	ic_proxy_prev_addrs = ic_proxy_addrs;
	ic_proxy_addrs = NULL;

	/* cancel any unfinished getaddrinfo reqs */
	{
		ListCell   *cell;

		foreach(cell, ic_proxy_unknown_addrs)
		{
			ICProxyAddr *addr = lfirst(cell);

			uv_cancel((uv_req_t *) &addr->req);
			ic_proxy_free(addr);
		}

		list_free(ic_proxy_unknown_addrs);
		ic_proxy_unknown_addrs = NIL;

		ic_proxy_my_addr = NULL;
	}

	/* parse the new addresses */
	{
		int			size = strlen(gp_interconnect_proxy_addresses) + 1;
		char	   *buf;
		FILE	   *f;
		int			dbid;
		int			content;
		int			port;
		char		hostname[HOST_NAME_MAX];
		struct addrinfo hints;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = 0;
		hints.ai_flags = 0;

		buf = ic_proxy_alloc(size);
		memcpy(buf, gp_interconnect_proxy_addresses, size);

		f = fmemopen(buf, size, "r");

		/*
		 * format: dbid:segid:hostname:port
		 */
		while (fscanf(f, "%d:%d:%[^:]:%d,",
					  &dbid, &content, hostname, &port) == 4)
		{
			ICProxyAddr *addr = ic_proxy_new(ICProxyAddr);

			addr->dbid = dbid;
			addr->content = content;
			snprintf(addr->hostname, sizeof(addr->hostname), "%s", hostname);
			snprintf(addr->service, sizeof(addr->service), "%d", port);
			ic_proxy_unknown_addrs = lappend(ic_proxy_unknown_addrs, addr);

			if (dbid == GpIdentity.dbid)
				ic_proxy_my_addr = addr;

			ic_proxy_log(LOG,
						 "ic-proxy-addr: seg%d,dbid%d: parsed addr: %s:%d",
						 content, dbid, hostname, port);

			uv_getaddrinfo(loop, &addr->req, ic_proxy_addr_on_getaddrinfo,
						   addr->hostname, addr->service, &hints);
		}

		fclose(f);
		ic_proxy_free(buf);
	}

	/* sort the new addrs so it's easy to diff */
	ic_proxy_unknown_addrs = list_qsort(ic_proxy_unknown_addrs,
										ic_proxy_addr_compare_dbid, NULL);

	/* the last thing is to classify the addrs */
	ic_proxy_classify_addresses(ic_proxy_prev_addrs /* oldaddrs */,
								ic_proxy_unknown_addrs /* newaddrs */);
}

/*
 * Get the proxy addr of the current segment.
 *
 * Return NULL if cannot find the addr.
 */
const ICProxyAddr *
ic_proxy_get_my_addr(void)
{
	if (ic_proxy_my_addr)
		return ic_proxy_my_addr;

	ic_proxy_log(LOG, "ic-proxy-addr: cannot get my addr");
	return NULL;
}

/*
 * Get the port from an address.
 *
 * Return -1 if cannot find the port.
 */
int
ic_proxy_addr_get_port(const ICProxyAddr *addr)
{
	if (addr->sockaddr.ss_family == AF_INET)
		return ntohs(((struct sockaddr_in *) addr)->sin_port);
	else if (addr->sockaddr.ss_family == AF_INET6)
		return ntohs(((struct sockaddr_in6 *) addr)->sin6_port);

	ic_proxy_log(WARNING,
				 "ic-proxy-addr: invalid address family %d for seg%d,dbid%d",
				 addr->sockaddr.ss_family, addr->content, addr->dbid);
	return -1;
}

/*
 * Extract the name and port from a sockaddr.
 *
 * - the hostname is stored in "name", the recommended size is HOST_NAME_MAX;
 * - the "namelen" is the buffer size of "name";
 * - the port is stored in "port";
 * - the address family is stored in "family" if it is not NULL;
 *
 * "name" and "port" must be provided, "family" is optional.
 *
 * Return 0 on success; otherwise return a negative value, which can be
 * translated with uv_strerror().  The __out__ fields are always filled.
 *
 * Failures from this function can be safely ignored, if the "addr" is really
 * bad, the "uv_tcp_bind()" or "uv_tcp_connect()" will fail with the actual
 * error code.
 */
int
ic_proxy_extract_sockaddr(const struct sockaddr *addr,
						  char *name, size_t namelen, int *port, int *family)
{
	int			ret;

	if (family)
		*family = addr->sa_family;

	switch (addr->sa_family)
	{
		case AF_INET:
			{
				const struct sockaddr_in *addr4
					= (const struct sockaddr_in *) addr;

				ret = uv_ip4_name(addr4, name, namelen);
				if (ret == 0)
					*port = ntohs(addr4->sin_port);
			}
			break;

		case AF_INET6:
			{
				const struct sockaddr_in6 *addr6
					= (const struct sockaddr_in6 *) addr;

				ret = uv_ip6_name(addr6, name, namelen);
				if (ret == 0)
					*port = ntohs(addr6->sin6_port);
			}
			break;

		default:
			ret = UV_EINVAL;
			break;
	}

	if (ret < 0)
	{
		snprintf(name, namelen, "unknown");
		*port = 0;
	}

	return ret;
}
