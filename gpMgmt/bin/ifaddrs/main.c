/*
 * ifaddrs - prints the routable IP addresses for all interfaces on the local
 *           system. Link-local addresses are never printed.
 *
 * Options:
 *     --no-loopback: suppresses output for loopback interfaces
 */

#include <arpa/inet.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdbool.h>

int main(int argc, char *argv[])
{
	struct ifaddrs *list;
	int		err;
	int		c;

	/* Options */
	int		no_loopback = 0;

	struct option opts[] = {
		{"no-loopback", no_argument, &no_loopback, 1},
		{ NULL },
	};

	while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1)
	{
		switch (c)
		{
			case 0:
				/* flag assignment already handled */
				break;

			default:
				fprintf(stderr, "usage: %s [--no-loopback]\n", argv[0]);
				return 1;
		}
	}

	err = getifaddrs(&list);
	if (err)
	{
		perror("getifaddrs");
		return 1;
	}

	for (; list; list = list->ifa_next)
	{
		const void *netaddr; /* the address to be converted to a string */
		char		addrbuf[INET6_ADDRSTRLEN];
		const char *addrstr;

		struct sockaddr_in	   *addr4;
		struct sockaddr_in6	   *addr6;
		struct sockaddr		   *addr = list->ifa_addr;

		bool exclude_loopback = (no_loopback && (list->ifa_flags & IFF_LOOPBACK));

		if (addr == NULL)
			continue;

		switch (addr->sa_family)
		{
			case AF_INET:
				addr4 = (struct sockaddr_in *) addr;

				/* Exclude 127.0.0.0/8 ip range */
				if (exclude_loopback)
				{
					uint32_t saddr = ntohl(addr4->sin_addr.s_addr);
					uint8_t b1;
					b1 = (uint8_t)(saddr >> 24 & 0xff);
					if (b1 == 127)
						continue;
				}

				netaddr = &addr4->sin_addr;
				break;

			case AF_INET6:
				addr6 = (struct sockaddr_in6 *) addr;
				if (addr6->sin6_scope_id)
				{
					/* Don't print out link-local addresses. */
					continue;
				}

				/* Exclude ::1 */
				if (exclude_loopback && IN6_IS_ADDR_LOOPBACK(&addr6->sin6_addr))
					continue;

				netaddr = &addr6->sin6_addr;
				break;

			default:
				/* we only care about IPv4/6 */
				continue;
		}

		/* Convert the address to a human-readable string */
		addrstr = inet_ntop(addr->sa_family, netaddr, addrbuf, sizeof(addrbuf));
		if (!addrstr)
		{
			perror("inet_ntop");
			return 1;
		}

		printf("%s\n", addrstr);
	}

	return 0;
}
