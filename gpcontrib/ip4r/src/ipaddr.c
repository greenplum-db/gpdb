/* ipaddr.c */

#include "postgres.h"

#include <math.h>
#include <sys/socket.h>

#include "fmgr.h"
#include "funcapi.h"

#include "access/gist.h"
#include "access/hash.h"
#include "access/skey.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"
#include "utils/elog.h"
#include "utils/numeric.h"
#include "utils/palloc.h"
#include "utils/varbit.h"

#include "ipr_internal.h"

#include "ip4r_funcs.h"
#include "ip6r_funcs.h"

void ipaddr_internal_error(void)
{
	elog(ERROR,"Invalid IP datum");

	/* just to shut the compiler up */
	abort();
}


/*
 * generic transforms of IP values - apply the specified ip4 or ip6 function
 * to the given args according to parameter type. Where the result type is an
 * IP, it's the same family as the parameter IP.
 */

/* func(IP) returns Datum */

static inline
Datum
ipaddr_transform_1d(Datum d, PGFunction ip4func, PGFunction ip6func)
{
    IP_P ipp = DatumGetIP_P(d);
	IP ip;
	int af = ip_unpack(ipp, &ip);

	switch (af)
	{
		case PGSQL_AF_INET:
			return DirectFunctionCall1(ip4func, IP4GetDatum(ip.ip4));

		case PGSQL_AF_INET6:
			return DirectFunctionCall1(ip6func, IP6PGetDatum(&ip.ip6));
	}

	ipaddr_internal_error();
}


/* func(IP) returns IP */

static inline
IP_P
ipaddr_transform_1(Datum d, PGFunction ip4func, PGFunction ip6func)
{
    IP_P ipp = DatumGetIP_P(d);
	IP ip;
	int af = ip_unpack(ipp, &ip);

	switch (af)
	{
		case PGSQL_AF_INET:
			ip.ip4 = DatumGetIP4(DirectFunctionCall1(ip4func, IP4GetDatum(ip.ip4)));
			break;

		case PGSQL_AF_INET6:
			ip.ip6 = *(DatumGetIP6P(DirectFunctionCall1(ip6func, IP6PGetDatum(&ip.ip6))));
			break;

		default:
			ipaddr_internal_error();
	}

    return ip_pack(af, &ip);
}


/* func(IP, Datum) returns IP */

static inline
IP_P
ipaddr_transform_2d(Datum d1, Datum d2, PGFunction ip4func, PGFunction ip6func)
{
    IP_P ipp = DatumGetIP_P(d1);
	IP ip;
	int af = ip_unpack(ipp, &ip);

	switch (af)
	{
		case PGSQL_AF_INET:
			ip.ip4 = DatumGetIP4(DirectFunctionCall2(ip4func, IP4GetDatum(ip.ip4), d2));
			break;

		case PGSQL_AF_INET6:
			ip.ip6 = *(DatumGetIP6P(DirectFunctionCall2(ip6func, IP6PGetDatum(&ip.ip6), d2)));
			break;

		default:
			ipaddr_internal_error();
	}

    return ip_pack(af, &ip);
}

/* func(IP,IP) returns IP; it's an error for the source IPs to be in different families */

static inline
IP_P
ipaddr_transform_2(Datum d1, Datum d2, PGFunction ip4func, PGFunction ip6func)
{
    IP_P ipp1 = DatumGetIP_P(d1);
    IP_P ipp2 = DatumGetIP_P(d2);
	IP ip1;
	IP ip2;
	IP out;
	int af1 = ip_unpack(ipp1, &ip1);
	int af2 = ip_unpack(ipp2, &ip2);

	if (af1 != af2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid mixing of IP address families")));

	switch (af1)
	{
		case PGSQL_AF_INET:
			out.ip4 = DatumGetIP4(DirectFunctionCall2(ip4func, IP4GetDatum(ip1.ip4), IP4GetDatum(ip2.ip4)));
			break;

		case PGSQL_AF_INET6:
			out.ip6 = *(DatumGetIP6P(DirectFunctionCall2(ip6func, IP6PGetDatum(&ip1.ip6), IP6PGetDatum(&ip2.ip6))));
			break;

		default:
			ipaddr_internal_error();
	}

    return ip_pack(af1, &out);
}




/*
#define GIST_DEBUG
#define GIST_QUERY_DEBUG
*/

static
text *
make_text(char *str, int len)
{
    text *ret = (text *) palloc(len + VARHDRSZ);
    SET_VARSIZE(ret, len + VARHDRSZ);
    if (str)
        memcpy(VARDATA(ret), str, len);
    else
        memset(VARDATA(ret), 0, len);
    return ret;
}

static inline
void
set_text_len(text *txt, int len)
{
    if ((len + VARHDRSZ) < VARSIZE(txt))
      SET_VARSIZE(txt, len + VARHDRSZ);
}


/*
** Input/Output routines
*/

PG_FUNCTION_INFO_V1(ipaddr_in);
Datum
ipaddr_in(PG_FUNCTION_ARGS)
{
    char *str = PG_GETARG_CSTRING(0);
	IP ip;

	if (strchr(str,':'))
	{
		if (ip6_raw_input(str, ip.ip6.bits))
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET6, &ip));
	}
	else
	{
		if (ip4_raw_input(str, &ip.ip4))
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET, &ip));
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP value: '%s'", str)));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ipaddr_out);
Datum
ipaddr_out(PG_FUNCTION_ARGS)
{
    IP_P ipp = PG_GETARG_IP_P(0);
    char *out = palloc(IP6_STRING_MAX);
	IP ip;

	switch (ip_unpack(ipp, &ip))
	{
		case PGSQL_AF_INET:
			ip4_raw_output(ip.ip4, out, IP6_STRING_MAX);
			break;
		case PGSQL_AF_INET6:
			ip6_raw_output(ip.ip6.bits, out, IP6_STRING_MAX);
			break;
	}

    PG_RETURN_CSTRING(out);
}

PG_FUNCTION_INFO_V1(ipaddr_recv);
Datum
ipaddr_recv(PG_FUNCTION_ARGS)
{
    StringInfo buf = (StringInfo) PG_GETARG_POINTER(0);
	IP ip;
	int af, bits, nbytes;

	/* we copy the external format used by inet/cidr, just because. */

	af = pq_getmsgbyte(buf);
	if (af != PGSQL_AF_INET && af != PGSQL_AF_INET6)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
				 errmsg("invalid address family in external IP value")));
	bits = pq_getmsgbyte(buf);
	if (bits != ipr_af_maxbits(af))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
				 errmsg("invalid bit length in external IP value")));
	(void) pq_getmsgbyte(buf);  /* ignore flag */
	nbytes = pq_getmsgbyte(buf);
	if (nbytes*8 != bits)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
				 errmsg("invalid address length in external IP value")));

	switch (af)
	{
		case PGSQL_AF_INET:
			ip.ip4 = (IP4) pq_getmsgint(buf, sizeof(IP4));
			break;

		case PGSQL_AF_INET6:
			ip.ip6.bits[0] = pq_getmsgint64(buf);
			ip.ip6.bits[1] = pq_getmsgint64(buf);
			break;
	}

	PG_RETURN_IP_P(ip_pack(af, &ip));
}

PG_FUNCTION_INFO_V1(ipaddr_send);
Datum
ipaddr_send(PG_FUNCTION_ARGS)
{
    IP_P arg1 = PG_GETARG_IP_P(0);
    StringInfoData buf;
	IP ip;
	int af = ip_unpack(arg1, &ip);

    pq_begintypsend(&buf);
	pq_sendbyte(&buf, af);
	pq_sendbyte(&buf, (int8) ipr_af_maxbits(af));
	pq_sendbyte(&buf, 1);
	pq_sendbyte(&buf, ip_sizeof(af));

	switch (af)
	{
		case PGSQL_AF_INET:
			pq_sendint(&buf, ip.ip4, sizeof(IP4));
			break;

		case PGSQL_AF_INET6:
			pq_sendint64(&buf, ip.ip6.bits[0]);
			pq_sendint64(&buf, ip.ip6.bits[1]);
			break;
	}

    PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

PG_FUNCTION_INFO_V1(ipaddr_hash);
Datum
ipaddr_hash(PG_FUNCTION_ARGS)
{
    IP_P arg1 = PG_GETARG_IP_P(0);

    return hash_any((void*)(VARDATA_ANY(arg1)), VARSIZE_ANY_EXHDR(arg1));
}

PG_FUNCTION_INFO_V1(ipaddr_hash_extended);
Datum
ipaddr_hash_extended(PG_FUNCTION_ARGS)
{
    IP_P arg1 = PG_GETARG_IP_P(0);
	uint64 seed = DatumGetUInt64(PG_GETARG_DATUM(1));

    return hash_any_extended((void*)(VARDATA_ANY(arg1)), VARSIZE_ANY_EXHDR(arg1), seed);
}

PG_FUNCTION_INFO_V1(ipaddr_cast_to_text);
Datum
ipaddr_cast_to_text(PG_FUNCTION_ARGS)
{
    IP_P ipp = PG_GETARG_IP_P(0);
	IP ip;
	int af = ip_unpack(ipp, &ip);
    text *out = NULL;

	switch (af)
	{
		case PGSQL_AF_INET:
			out = make_text(NULL, IP4_STRING_MAX);
			set_text_len(out, ip4_raw_output(ip.ip4, VARDATA(out), IP4_STRING_MAX));
			break;
		case PGSQL_AF_INET6:
			out = make_text(NULL, IP6_STRING_MAX);
			set_text_len(out, ip6_raw_output(ip.ip6.bits, VARDATA(out), IP6_STRING_MAX));
			break;
	}

    PG_RETURN_TEXT_P(out);
}

PG_FUNCTION_INFO_V1(ipaddr_cast_from_text);
Datum
ipaddr_cast_from_text(PG_FUNCTION_ARGS)
{
    text *txt = PG_GETARG_TEXT_PP(0);
    int tlen = VARSIZE_ANY_EXHDR(txt);
    char buf[IP6_STRING_MAX];

    if (tlen < sizeof(buf))
    {
		IP ip;

        memcpy(buf, VARDATA_ANY(txt), tlen);
        buf[tlen] = 0;

		if (strchr(buf,':'))
		{
			if (ip6_raw_input(buf, ip.ip6.bits))
				PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET6, &ip));
		}
		else
		{
			if (ip4_raw_input(buf, &ip.ip4))
				PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET, &ip));
		}
    }

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP value in text")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ipaddr_cast_from_inet);
Datum
ipaddr_cast_from_inet(PG_FUNCTION_ARGS)
{
    inet *inetptr = PG_GETARG_INET_P(0);
    inet_struct *in = INET_STRUCT_DATA(inetptr);
	IP ip;

	switch (in->family)
	{
		case PGSQL_AF_INET:
			ip.ip4 = DatumGetIP4(DirectFunctionCall1(ip4_cast_from_inet, InetPGetDatum(inetptr)));
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET, &ip));
		case PGSQL_AF_INET6:
			ip.ip6 = *(DatumGetIP6P(DirectFunctionCall1(ip6_cast_from_inet, InetPGetDatum(inetptr))));
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET6, &ip));
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid INET value for conversion to IP")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ipaddr_cast_to_cidr);
Datum
ipaddr_cast_to_cidr(PG_FUNCTION_ARGS)
{
	PG_RETURN_DATUM(ipaddr_transform_1d(PG_GETARG_DATUM(0), ip4_cast_to_cidr, ip6_cast_to_cidr));
}


PG_FUNCTION_INFO_V1(ipaddr_cast_to_numeric);
Datum
ipaddr_cast_to_numeric(PG_FUNCTION_ARGS)
{
	PG_RETURN_DATUM(ipaddr_transform_1d(PG_GETARG_DATUM(0), ip4_cast_to_numeric, ip6_cast_to_numeric));
}

PG_FUNCTION_INFO_V1(ipaddr_cast_from_ip4);
Datum
ipaddr_cast_from_ip4(PG_FUNCTION_ARGS)
{
	IP ip;

	ip.ip4 = PG_GETARG_IP4(0);

	PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET, &ip));
}

PG_FUNCTION_INFO_V1(ipaddr_cast_from_ip6);
Datum
ipaddr_cast_from_ip6(PG_FUNCTION_ARGS)
{
	IP6 *in = PG_GETARG_IP6_P(0);
	IP ip;

	ip.ip6 = *in;

	PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET6, &ip));
}


PG_FUNCTION_INFO_V1(ipaddr_cast_to_ip4);
Datum
ipaddr_cast_to_ip4(PG_FUNCTION_ARGS)
{
	IP_P ipp = PG_GETARG_IP_P(0);
	IP ip;

	if (ip_unpack(ipp, &ip) == PGSQL_AF_INET)
	{
		PG_RETURN_IP4(ip.ip4);
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP value in cast to IP4")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ipaddr_cast_to_ip6);
Datum
ipaddr_cast_to_ip6(PG_FUNCTION_ARGS)
{
	IP_P ipp = PG_GETARG_IP_P(0);
	IP ip;

	if (ip_unpack(ipp, &ip) == PGSQL_AF_INET6)
	{
		IP6 *out = palloc(sizeof(IP6));
		*out = ip.ip6;
		PG_RETURN_IP6_P(out);
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP value in cast to IP4")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ipaddr_cast_from_bit);
Datum
ipaddr_cast_from_bit(PG_FUNCTION_ARGS)
{
	VarBit *val = PG_GETARG_VARBIT_P(0);
	IP ip;

	switch (VARBITLEN(val))
	{
		case 32:
			ip.ip4 = DatumGetIP4(DirectFunctionCall1(ip4_cast_from_bit, VarBitPGetDatum(val)));
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET, &ip));
		case 128:
			ip.ip6 = *(DatumGetIP6P(DirectFunctionCall1(ip6_cast_from_bit, VarBitPGetDatum(val))));
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET6, &ip));
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid BIT value for conversion to IPADDRESS")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ipaddr_cast_to_bit);
Datum
ipaddr_cast_to_bit(PG_FUNCTION_ARGS)
{
	PG_RETURN_DATUM(ipaddr_transform_1d(PG_GETARG_DATUM(0), ip4_cast_to_bit, ip6_cast_to_bit));
}

PG_FUNCTION_INFO_V1(ipaddr_cast_from_bytea);
Datum
ipaddr_cast_from_bytea(PG_FUNCTION_ARGS)
{
	void *val = PG_GETARG_BYTEA_PP(0);
	IP ip;

	switch (VARSIZE_ANY_EXHDR(val))
	{
		case 4:
			ip.ip4 = DatumGetIP4(DirectFunctionCall1(ip4_cast_from_bytea, PointerGetDatum(val)));
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET, &ip));
		case 16:
			ip.ip6 = *(DatumGetIP6P(DirectFunctionCall1(ip6_cast_from_bytea, PointerGetDatum(val))));
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET6, &ip));
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid BYTEA value for conversion to IPADDRESS")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ipaddr_cast_to_bytea);
Datum
ipaddr_cast_to_bytea(PG_FUNCTION_ARGS)
{
	PG_RETURN_DATUM(ipaddr_transform_1d(PG_GETARG_DATUM(0), ip4_cast_to_bytea, ip6_cast_to_bytea));
}


PG_FUNCTION_INFO_V1(ipaddr_family);
Datum
ipaddr_family(PG_FUNCTION_ARGS)
{
	IP_P ipp = PG_GETARG_IP_P(0);
	IP ip;

	if (ip_unpack(ipp, &ip) == PGSQL_AF_INET6)
		PG_RETURN_INT32(6);
	else
		PG_RETURN_INT32(4);
}


PG_FUNCTION_INFO_V1(ipaddr_net_lower);
Datum
ipaddr_net_lower(PG_FUNCTION_ARGS)
{
    PG_RETURN_IP_P(ipaddr_transform_2d(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_net_lower, ip6_net_lower));
}

PG_FUNCTION_INFO_V1(ipaddr_net_upper);
Datum
ipaddr_net_upper(PG_FUNCTION_ARGS)
{
    PG_RETURN_IP_P(ipaddr_transform_2d(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_net_upper, ip6_net_upper));
}

PG_FUNCTION_INFO_V1(ipaddr_plus_int);
Datum
ipaddr_plus_int(PG_FUNCTION_ARGS)
{
    PG_RETURN_IP_P(ipaddr_transform_2d(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_plus_int, ip6_plus_int));
}

PG_FUNCTION_INFO_V1(ipaddr_plus_bigint);
Datum
ipaddr_plus_bigint(PG_FUNCTION_ARGS)
{
    PG_RETURN_IP_P(ipaddr_transform_2d(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_plus_bigint, ip6_plus_bigint));
}

PG_FUNCTION_INFO_V1(ipaddr_plus_numeric);
Datum
ipaddr_plus_numeric(PG_FUNCTION_ARGS)
{
    PG_RETURN_IP_P(ipaddr_transform_2d(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_plus_numeric, ip6_plus_numeric));
}

PG_FUNCTION_INFO_V1(ipaddr_minus_int);
Datum
ipaddr_minus_int(PG_FUNCTION_ARGS)
{
    PG_RETURN_IP_P(ipaddr_transform_2d(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_minus_int, ip6_minus_int));
}

PG_FUNCTION_INFO_V1(ipaddr_minus_bigint);
Datum
ipaddr_minus_bigint(PG_FUNCTION_ARGS)
{
    PG_RETURN_IP_P(ipaddr_transform_2d(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_minus_bigint, ip6_minus_bigint));
}

PG_FUNCTION_INFO_V1(ipaddr_minus_numeric);
Datum
ipaddr_minus_numeric(PG_FUNCTION_ARGS)
{
    PG_RETURN_IP_P(ipaddr_transform_2d(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_minus_numeric, ip6_minus_numeric));
}

PG_FUNCTION_INFO_V1(ipaddr_minus_ipaddr);
Datum
ipaddr_minus_ipaddr(PG_FUNCTION_ARGS)
{
    Datum minuend = PG_GETARG_DATUM(0);
    Datum subtrahend = PG_GETARG_DATUM(1);
    Datum res;
	IP ip1;
	IP ip2;
	int af1 = ip_unpack(DatumGetIP_P(minuend), &ip1);
	int af2 = ip_unpack(DatumGetIP_P(subtrahend), &ip2);

	if (af1 != af2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid mixing of IP address families")));

	switch (af1)
	{
		case PGSQL_AF_INET:
			res = DirectFunctionCall2(numeric_sub,
									  DirectFunctionCall1(ip4_cast_to_numeric,IP4GetDatum(ip1.ip4)),
									  DirectFunctionCall1(ip4_cast_to_numeric,IP4GetDatum(ip2.ip4)));
			break;

		case PGSQL_AF_INET6:
			res = DirectFunctionCall2(numeric_sub,
									  DirectFunctionCall1(ip6_cast_to_numeric,IP6PGetDatum(&ip1.ip6)),
									  DirectFunctionCall1(ip6_cast_to_numeric,IP6PGetDatum(&ip2.ip6)));
			break;

		default:
			ipaddr_internal_error();
	}

    PG_RETURN_DATUM(res);
}

PG_FUNCTION_INFO_V1(ipaddr_and);
Datum
ipaddr_and(PG_FUNCTION_ARGS)
{
	PG_RETURN_IP_P(ipaddr_transform_2(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_and, ip6_and));
}

PG_FUNCTION_INFO_V1(ipaddr_or);
Datum
ipaddr_or(PG_FUNCTION_ARGS)
{
	PG_RETURN_IP_P(ipaddr_transform_2(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_or, ip6_or));
}

PG_FUNCTION_INFO_V1(ipaddr_xor);
Datum
ipaddr_xor(PG_FUNCTION_ARGS)
{
	PG_RETURN_IP_P(ipaddr_transform_2(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), ip4_xor, ip6_xor));
}

PG_FUNCTION_INFO_V1(ipaddr_not);
Datum
ipaddr_not(PG_FUNCTION_ARGS)
{
	PG_RETURN_IP_P(ipaddr_transform_1(PG_GETARG_DATUM(0), ip4_not, ip6_not));
}


/*
 * generic boolean comparison of two IPs. If in the same family, then the
 * passed-in comparison function is called; if in different families, then
 * the result is mismatch_af1 or _af2 according to which of the first or
 * second address is in the larger family.
 */

static inline
bool
ipaddr_comparison_bool(Datum d1, Datum d2,
					   bool mismatch_af1, bool mismatch_af2,
					   PGFunction ip4func, PGFunction ip6func)
{
    IP_P ipp1 = DatumGetIP_P(d1);
    IP_P ipp2 = DatumGetIP_P(d2);
	IP ip1;
	IP ip2;
	int af1 = ip_unpack(ipp1, &ip1);
	int af2 = ip_unpack(ipp2, &ip2);
	bool retval;

	if (af1 != af2)
	{
		retval = (af1 > af2) ? mismatch_af1 : mismatch_af2;
	}
	else
	{
		switch (af1)
		{
			case PGSQL_AF_INET:
				retval = DatumGetBool(DirectFunctionCall2(ip4func, IP4GetDatum(ip1.ip4), IP4GetDatum(ip2.ip4)));
				break;

			case PGSQL_AF_INET6:
				retval = DatumGetBool(DirectFunctionCall2(ip6func, IP6PGetDatum(&ip1.ip6), IP6PGetDatum(&ip2.ip6)));
				break;

			default:
				ipaddr_internal_error();
		}
	}

	if ((Pointer)ipp1 != DatumGetPointer(d1))
		pfree(ipp1);
	if ((Pointer)ipp2 != DatumGetPointer(d2))
		pfree(ipp2);

    return retval;
}

PG_FUNCTION_INFO_V1(ipaddr_lt);
Datum
ipaddr_lt(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
		ipaddr_comparison_bool(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1),
							   false, true,
							   ip4_lt, ip6_lt));
}

PG_FUNCTION_INFO_V1(ipaddr_le);
Datum
ipaddr_le(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
		ipaddr_comparison_bool(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1),
							   false, true,
							   ip4_le, ip6_le));
}

PG_FUNCTION_INFO_V1(ipaddr_gt);
Datum
ipaddr_gt(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
		ipaddr_comparison_bool(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1),
							   true, false,
							   ip4_gt, ip6_gt));
}

PG_FUNCTION_INFO_V1(ipaddr_ge);
Datum
ipaddr_ge(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
		ipaddr_comparison_bool(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1),
							   true, false,
							   ip4_ge, ip6_ge));
}

PG_FUNCTION_INFO_V1(ipaddr_eq);
Datum
ipaddr_eq(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
		ipaddr_comparison_bool(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1),
							   false, false,
							   ip4_eq, ip6_eq));
}

PG_FUNCTION_INFO_V1(ipaddr_neq);
Datum
ipaddr_neq(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(
		ipaddr_comparison_bool(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1),
							   true, true,
							   ip4_neq, ip6_neq));
}


/*****************************************************************************
 *                                                 Btree functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(ipaddr_cmp);
Datum
ipaddr_cmp(PG_FUNCTION_ARGS)
{
    IP_P ipp1 = PG_GETARG_IP_P(0);
	IP_P ipp2 = PG_GETARG_IP_P(1);
	IP ip1;
	IP ip2;
	int af1 = ip_unpack(ipp1, &ip1);
	int af2 = ip_unpack(ipp2, &ip2);
	int32 retval;

	if (af1 != af2)
	{
		retval = (af1 > af2) ? 1 : -1;
	}
	else
	{
		switch (af1)
		{
			case PGSQL_AF_INET:
				retval = DatumGetInt32(DirectFunctionCall2(ip4_cmp, IP4GetDatum(ip1.ip4), IP4GetDatum(ip2.ip4)));
				break;

			case PGSQL_AF_INET6:
				retval = DatumGetInt32(DirectFunctionCall2(ip6_cmp, IP6PGetDatum(&ip1.ip6), IP6PGetDatum(&ip2.ip6)));
				break;

			default:
				ipaddr_internal_error();
		}
	}

	PG_FREE_IF_COPY(ipp1,0);
	PG_FREE_IF_COPY(ipp2,1);

    PG_RETURN_INT32(retval);
}

/* end */
