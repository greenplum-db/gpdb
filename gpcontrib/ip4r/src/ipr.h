/* ipr.h */
#ifndef IPR_H
#define IPR_H

#include <string.h>
#include <sys/socket.h>

#include "fmgr.h"

#include "utils/inet.h"
#include "utils/palloc.h"

#if !defined(PG_VERSION_NUM)
#error "Unknown or unsupported postgresql version"
#endif
#if PG_VERSION_NUM < 80400
#error "Unknown or unsupported postgresql version"
#endif

#ifndef PGDLLEXPORT
#define PGDLLEXPORT
#endif

PGDLLEXPORT bool ip4_raw_input(const char *src, uint32 *dst);
PGDLLEXPORT bool ip6_raw_input(const char *src, uint64 *dst);
PGDLLEXPORT int ip4_raw_output(uint32 ip, char *str, int len);
PGDLLEXPORT int ip6_raw_output(uint64 *ip, char *str, int len);

/* IP4 = uint32, stored in host-order. fixed-length and pass by value. */
typedef uint32 IP4;

#define IP4_INITIALIZER 0

/* IP4R = range of IP4, stored in host-order. fixed-length by reference */
typedef struct IP4R {
    IP4 lower;
    IP4 upper;
} IP4R;

#define IP4R_INITIALIZER {0,0}

/*
 * IP6 = 2 x uint64, stored hi to lo, each stored in host-order.
 * fixed-length and pass by reference.
 */
typedef struct IP6 {
    uint64 bits[2];
} IP6;

#define IP6_INITIALIZER {{0,0}}

/* IP6R = range of IP6. fixed-length by reference */
typedef struct IP6R {
    IP6 lower;
    IP6 upper;
} IP6R;

#define IP6R_INITIALIZER {IP6_INITIALIZER,IP6_INITIALIZER}

#define IP6_STRING_MAX (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")+2)
#define IP6R_STRING_MAX (2*IP6_STRING_MAX)

#define IP4_STRING_MAX (sizeof("255.255.255.255"))
#define IP4R_STRING_MAX (2*IP4_STRING_MAX)

typedef union IP {
	IP6 ip6;
	IP4 ip4;
} IP;

#define IP_INITIALIZER {IP6_INITIALIZER}

#define ipr_af_maxbits(af_) ((af_) == PGSQL_AF_INET ? 32 : 128)
#define ip_sizeof(af_) ((af_) == PGSQL_AF_INET ? sizeof(IP4) : sizeof(IP6))
#define ipr_sizeof(af_) ((af_) == PGSQL_AF_INET ? sizeof(IP4R) : sizeof(IP6R))

typedef void *IP_P;  /* unaligned! */

PGDLLEXPORT void ipaddr_internal_error(void) __attribute__((noreturn));

static inline
int ip_unpack(IP_P in, IP *out)
{
    switch (VARSIZE_ANY_EXHDR(in))
    {
        case sizeof(IP4):
            memcpy(&out->ip4, VARDATA_ANY(in), sizeof(IP4));
            return PGSQL_AF_INET;
        case sizeof(IP6):
            memcpy(&out->ip6, VARDATA_ANY(in), sizeof(IP6));
            return PGSQL_AF_INET6;
        default:
			ipaddr_internal_error();
    }
}

static inline
IP_P ip_pack(int af, IP *val)
{
	int sz = ip_sizeof(af);
    IP_P out = palloc(VARHDRSZ + sz);

	SET_VARSIZE(out, VARHDRSZ + sz);
	memcpy(VARDATA(out), val, sz);
	return out;
}

typedef union IPR {
	IP6R ip6r;
	IP4R ip4r;
} IPR;

#define IPR_INITIALIZER {IP6R_INITIALIZER}

typedef void *IPR_P;  /* unaligned! */

PGDLLEXPORT int ipr_unpack(IPR_P in, IPR *out);
PGDLLEXPORT IPR_P ipr_pack(int af, IPR *val);

#define DatumGetIP4RP(X)	((IP4R *) DatumGetPointer(X))
#define IP4RPGetDatum(X)	PointerGetDatum(X)
#define PG_GETARG_IP4R_P(n) DatumGetIP4RP(PG_GETARG_DATUM(n))
#define PG_RETURN_IP4R_P(x) return IP4RPGetDatum(x)

#define DatumGetIP4(X) DatumGetUInt32(X)
#define IP4GetDatum(X) UInt32GetDatum(X)
#define PG_GETARG_IP4(n) PG_GETARG_UINT32(n)
#define PG_RETURN_IP4(x) PG_RETURN_UINT32(x)

#define DatumGetIP6RP(X)	((IP6R *) DatumGetPointer(X))
#define IP6RPGetDatum(X)	PointerGetDatum(X)
#define PG_GETARG_IP6R_P(n) DatumGetIP6RP(PG_GETARG_DATUM(n))
#define PG_RETURN_IP6R_P(x) return IP6RPGetDatum(x)

#define DatumGetIP6P(X)	((IP6 *) DatumGetPointer(X))
#define IP6PGetDatum(X)	PointerGetDatum(X)
#define PG_GETARG_IP6_P(n) DatumGetIP6P(PG_GETARG_DATUM(n))
#define PG_RETURN_IP6_P(x) return IP6PGetDatum(x)

#define DatumGetIP_P(X) ((IP_P) PG_DETOAST_DATUM_PACKED(X))
#define IP_PGetDatum(X) PointerGetDatum(X)
#define PG_GETARG_IP_P(n) DatumGetIP_P(PG_GETARG_DATUM(n))
#define PG_RETURN_IP_P(x) return IP_PGetDatum(x)

#define DatumGetIPR_P(X) ((IP_P) PG_DETOAST_DATUM_PACKED(X))
#define IPR_PGetDatum(X) PointerGetDatum(X)
#define PG_GETARG_IPR_P(n) DatumGetIPR_P(PG_GETARG_DATUM(n))
#define PG_RETURN_IPR_P(x) return IPR_PGetDatum(x)

#endif
/* end */
