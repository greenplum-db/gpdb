/* iprange.c */

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

/*
 * Some C compilers including GCC really get confused by the use of the IP and
 * IPR unions, generating a lot of completely spurious "may be used
 * uninitialized" warnings. The various uses of IP*_INITIALIZER in this file
 * ought to be unnecessary; so this macro is used to make them conditional for
 * ease of experimentation.
 */

#define XINIT(_i) = _i

static void iprange_internal_error(void) __attribute__((noreturn,noinline));
static void iprange_af_mismatch(void) __attribute__((noreturn,noinline));

static
void iprange_internal_error(void)
{
	elog(ERROR,"Invalid IPR datum");

	/* just to shut the compiler up */
	abort();
}

static
void iprange_af_mismatch(void)
{
	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("invalid mixing of IP address families")));
	/* just to shut the compiler up */
	abort();
}


/*
 * Rather than use 32 bytes + length for every IP range, we make the
 * assumption that most ranges (at least in table data and hence gist
 * leaf nodes) will be CIDRs, and many of those /64 or shorter.
 *
 * So we allow the following formats (excluding varlena header):
 *
 *  0 bytes                  - special 'match all' range (af==0)
 *  8 bytes                  - IP4R
 *  1 byte pfxlen + 8 bytes  - IP6R cidr range /64 or shorter
 *  1 byte pfxlen + 16 bytes - IP6R cidr range /65 or longer
 *  32 bytes                 - arbitrary IP6R range
 */

int ipr_unpack(IPR_P in, IPR *out)
{
	unsigned char *ptr = (unsigned char *) VARDATA_ANY(in);

    switch (VARSIZE_ANY_EXHDR(in))
    {
		case 0:
			return 0;

        case sizeof(IP4R):
            memcpy(&out->ip4r, ptr, sizeof(IP4R));
			return PGSQL_AF_INET;

		case 1+sizeof(uint64):
		{
			unsigned pfxlen = *ptr++;
			memcpy(out->ip6r.lower.bits, ptr, sizeof(uint64));
			out->ip6r.lower.bits[1] = 0;
			out->ip6r.upper.bits[0] = out->ip6r.lower.bits[0] | hostmask6_hi(pfxlen);
			out->ip6r.upper.bits[1] = hostmask6_lo(pfxlen);
			return PGSQL_AF_INET6;
		}

		case 1+sizeof(IP6):
		{
			unsigned pfxlen = *ptr++;
			memcpy(&out->ip6r.lower, ptr, sizeof(IP6));
			out->ip6r.upper.bits[0] = out->ip6r.lower.bits[0] | hostmask6_hi(pfxlen);
			out->ip6r.upper.bits[1] = out->ip6r.lower.bits[1] | hostmask6_lo(pfxlen);
			return PGSQL_AF_INET6;
		}

        case sizeof(IP6R):
            memcpy(&out->ip6r, ptr, sizeof(IP6R));
			return PGSQL_AF_INET6;

        default:
			iprange_internal_error();
    }
}

IPR_P ipr_pack(int af, IPR *val)
{
    IPR_P out = palloc(VARHDRSZ + sizeof(IP6R));
	unsigned char *ptr = (unsigned char *) VARDATA(out);

	switch (af)
	{
		case 0:
			SET_VARSIZE(out, VARHDRSZ);
			break;

		case PGSQL_AF_INET:
			memcpy(ptr, &val->ip4r, sizeof(IP4R));
			SET_VARSIZE(out, VARHDRSZ + sizeof(IP4R));
			break;

		case PGSQL_AF_INET6:
		{
			unsigned pfxlen = masklen6(&val->ip6r.lower, &val->ip6r.upper);
			if (pfxlen <= 64)
			{
				*ptr++ = pfxlen;
				memcpy(ptr, val->ip6r.lower.bits, sizeof(uint64));
				SET_VARSIZE(out, VARHDRSZ + 1 + sizeof(uint64));
			}
			else if (pfxlen <= 128)
			{
				*ptr++ = pfxlen;
				memcpy(ptr, &val->ip6r.lower, sizeof(IP6));
				SET_VARSIZE(out, VARHDRSZ + 1 + sizeof(IP6));
			}
			else
			{
				memcpy(ptr, &val->ip6r, sizeof(IP6R));
				SET_VARSIZE(out, VARHDRSZ + sizeof(IP6R));
			}
			break;
		}

		default:
			iprange_internal_error();
	}

	return out;
}


/**************************************************************************/
/* This part handles all aspects of postgres interfacing.
 */

/*
** Input/Output routines
*/

/*---- ipr ----*/

PG_FUNCTION_INFO_V1(iprange_in);
Datum
iprange_in(PG_FUNCTION_ARGS)
{
    char *str = PG_GETARG_CSTRING(0);
    IPR ipr;

	if (str[0] == '-' && str[1] == 0)
	{
		PG_RETURN_IPR_P(ipr_pack(0, NULL));
	}
	else if (strchr(str,':'))
	{
		ipr.ip6r = *DatumGetIP6RP(DirectFunctionCall1(ip6r_in,CStringGetDatum(str)));
		PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6, &ipr));
	}
	else
	{
		ipr.ip4r = *DatumGetIP4RP(DirectFunctionCall1(ip4r_in,CStringGetDatum(str)));
		PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET, &ipr));
	}
}

PG_FUNCTION_INFO_V1(iprange_out);
Datum
iprange_out(PG_FUNCTION_ARGS)
{
	IPR_P *iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(iprp, &ipr);

	switch (af)
	{
		case 0:
		{
			char *out = palloc(2);
			strcpy(out,"-");
			PG_RETURN_CSTRING(out);
		}

		case PGSQL_AF_INET:
			PG_RETURN_DATUM(DirectFunctionCall1(ip4r_out,IP4RPGetDatum(&ipr.ip4r)));

		case PGSQL_AF_INET6:
			PG_RETURN_DATUM(DirectFunctionCall1(ip6r_out,IP6RPGetDatum(&ipr.ip6r)));

		default:
			iprange_internal_error();
	}
}

PG_FUNCTION_INFO_V1(iprange_recv);
Datum
iprange_recv(PG_FUNCTION_ARGS)
{
    StringInfo buf = (StringInfo) PG_GETARG_POINTER(0);
    IPR ipr;
	unsigned af, bits, nbytes;

	/*
	 * This isn't quite the same format as inet/cidr but we keep reasonably
	 * close for no very good reason.
	 *
	 * 1 byte AF
	 * 1 byte pfx len (255 if the range is not a prefix)
	 * 1 byte flag (unused)
	 * 1 byte number of remaining bytes (0,4,8,16 or 32)
	 *
	 */

	af = pq_getmsgbyte(buf);
	if (af != 0 && af != PGSQL_AF_INET && af != PGSQL_AF_INET6)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
				 errmsg("invalid address family in external IPR value")));
	bits = pq_getmsgbyte(buf);
	if (bits != 255 && bits > ipr_af_maxbits(af))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
				 errmsg("invalid bit length in external IP value")));
	(void) pq_getmsgbyte(buf);  /* ignore flag */
	nbytes = pq_getmsgbyte(buf);

	switch (af)
	{
		case 0:  /* special 'match all' range */
			if (nbytes == 0)
				PG_RETURN_IPR_P(ipr_pack(0,NULL));
			break;

		case PGSQL_AF_INET:
			if (nbytes == sizeof(IP4) && bits <= ipr_af_maxbits(PGSQL_AF_INET))
			{
				ipr.ip4r.lower = (IP4) pq_getmsgint(buf, sizeof(IP4));
				ipr.ip4r.upper = ipr.ip4r.lower | hostmask(bits);
				PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET,&ipr));
			}
			else if (nbytes == sizeof(IP4R))
			{
				ipr.ip4r.lower = (IP4) pq_getmsgint(buf, sizeof(IP4));
				ipr.ip4r.upper = (IP4) pq_getmsgint(buf, sizeof(IP4));
				PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET,&ipr));
			}
			break;

		case PGSQL_AF_INET6:
			if (nbytes == sizeof(uint64) && bits <= 64)
			{
				ipr.ip6r.lower.bits[0] = (uint64) pq_getmsgint64(buf);
				ipr.ip6r.lower.bits[1] = 0;
				ipr.ip6r.upper.bits[0] = ipr.ip6r.lower.bits[0] | hostmask6_hi(bits);
				ipr.ip6r.upper.bits[1] = ~(uint64)0;
				PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6,&ipr));
			}
			else if (nbytes == sizeof(IP6) && bits <= ipr_af_maxbits(PGSQL_AF_INET6))
			{
				ipr.ip6r.lower.bits[0] = (uint64) pq_getmsgint64(buf);
				ipr.ip6r.lower.bits[1] = (uint64) pq_getmsgint64(buf);
				ipr.ip6r.upper.bits[0] = ipr.ip6r.lower.bits[0] | hostmask6_hi(bits);
				ipr.ip6r.upper.bits[1] = ipr.ip6r.lower.bits[1] | hostmask6_lo(bits);
				PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6,&ipr));
			}
			else if (nbytes == sizeof(IP6R))
			{
				ipr.ip6r.lower.bits[0] = (uint64) pq_getmsgint64(buf);
				ipr.ip6r.lower.bits[1] = (uint64) pq_getmsgint64(buf);
				ipr.ip6r.upper.bits[0] = (uint64) pq_getmsgint64(buf);
				ipr.ip6r.upper.bits[1] = (uint64) pq_getmsgint64(buf);
				PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6,&ipr));
			}
			break;
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
			 errmsg("invalid address length in external IPR value")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(iprange_send);
Datum
iprange_send(PG_FUNCTION_ARGS)
{
    IPR_P *iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(iprp, &ipr);
    StringInfoData buf;
	unsigned bits = ~0;

	switch (af)
	{
		case PGSQL_AF_INET:
			bits = masklen(ipr.ip4r.lower,ipr.ip4r.upper);
			break;
		case PGSQL_AF_INET6:
			bits = masklen6(&ipr.ip6r.lower,&ipr.ip6r.upper);
			break;
	}

    pq_begintypsend(&buf);
	pq_sendbyte(&buf, af);
	pq_sendbyte(&buf, (int8) bits);
	pq_sendbyte(&buf, 1);

	switch (af)
	{
		case 0:
			pq_sendbyte(&buf,0);
			break;

		case PGSQL_AF_INET:
			if (bits <= ipr_af_maxbits(PGSQL_AF_INET))
			{
				pq_sendbyte(&buf, sizeof(IP4));
				pq_sendint(&buf, ipr.ip4r.lower, sizeof(IP4));
			}
			else
			{
				pq_sendbyte(&buf, sizeof(IP4R));
				pq_sendint(&buf, ipr.ip4r.lower, sizeof(IP4));
				pq_sendint(&buf, ipr.ip4r.upper, sizeof(IP4));
			}
			break;

		case PGSQL_AF_INET6:
			if (bits <= 64)
			{
				pq_sendbyte(&buf, sizeof(uint64));
				pq_sendint64(&buf, ipr.ip6r.lower.bits[0]);
			}
			else if (bits <= ipr_af_maxbits(PGSQL_AF_INET6))
			{
				pq_sendbyte(&buf, sizeof(IP6));
				pq_sendint64(&buf, ipr.ip6r.lower.bits[0]);
				pq_sendint64(&buf, ipr.ip6r.lower.bits[1]);
			}
			else
			{
				pq_sendbyte(&buf, sizeof(IP6R));
				pq_sendint64(&buf, ipr.ip6r.lower.bits[0]);
				pq_sendint64(&buf, ipr.ip6r.lower.bits[1]);
				pq_sendint64(&buf, ipr.ip6r.upper.bits[0]);
				pq_sendint64(&buf, ipr.ip6r.upper.bits[1]);
			}
			break;
	}

    PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

/* Unfortunately due to a historical oversight, this function produces
 * different hash values for ipv6 cidr ranges than ip6rhash. This is
 * undesirable for a hash function since it would block future attempts to add
 * cross-type comparisons, and if this behaviour were to be propagated into
 * the extended hash function then it would become cast in stone, thanks to
 * hash partitioning.
 *
 * So we make the old sql-callable function name keep using the old hash, so
 * that anyone manually using iprangehash for partitioning or whatever is
 * unaffected, but change the actual hash opclass to use new names.
 */

PG_FUNCTION_INFO_V1(iprange_hash);
Datum
iprange_hash(PG_FUNCTION_ARGS)
{
    IPR_P arg1 = PG_GETARG_IPR_P(0);

    return hash_any((void *) VARDATA_ANY(arg1), VARSIZE_ANY_EXHDR(arg1));
}

/* below are the fixed hash functions
 */

PG_FUNCTION_INFO_V1(iprange_hash_new);
Datum
iprange_hash_new(PG_FUNCTION_ARGS)
{
    IPR_P arg1 = PG_GETARG_IPR_P(0);
	IPR tmp;
	uint32 vsize = VARSIZE_ANY_EXHDR(arg1);

	if (vsize <= sizeof(IP4R) || vsize == sizeof(IP6R))
		return hash_any((void *) VARDATA_ANY(arg1), vsize);

	if (ipr_unpack(arg1,&tmp) != PGSQL_AF_INET6)
		iprange_internal_error();

	return hash_any((void *) &tmp, sizeof(IP6R));
}

PG_FUNCTION_INFO_V1(iprange_hash_extended);
Datum
iprange_hash_extended(PG_FUNCTION_ARGS)
{
    IPR_P arg1 = PG_GETARG_IPR_P(0);
	IPR tmp;
	uint32 vsize = VARSIZE_ANY_EXHDR(arg1);
	uint32 seed = DatumGetUInt32(PG_GETARG_DATUM(1));

	if (vsize <= sizeof(IP4R) || vsize == sizeof(IP6R))
		return hash_any_extended((void *) VARDATA_ANY(arg1), vsize, seed);

	if (ipr_unpack(arg1,&tmp) != PGSQL_AF_INET6)
		iprange_internal_error();

	return hash_any_extended((void *) &tmp, sizeof(IP6R), seed);
}

PG_FUNCTION_INFO_V1(iprange_cast_to_text);
Datum
iprange_cast_to_text(PG_FUNCTION_ARGS)
{
    IPR_P iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(iprp,&ipr);

	switch (af)
	{
		case 0:
		{
			text *out = cstring_to_text_with_len("-",1);
			PG_RETURN_TEXT_P(out);
		}

		case PGSQL_AF_INET:
			PG_RETURN_DATUM(DirectFunctionCall1(ip4r_cast_to_text,IP4RPGetDatum(&ipr.ip4r)));

		case PGSQL_AF_INET6:
			PG_RETURN_DATUM(DirectFunctionCall1(ip6r_cast_to_text,IP6RPGetDatum(&ipr.ip6r)));

		default:
			iprange_internal_error();
	}
}

PG_FUNCTION_INFO_V1(iprange_cast_from_text);
Datum
iprange_cast_from_text(PG_FUNCTION_ARGS)
{
    text *txt = PG_GETARG_TEXT_PP(0);
    int tlen = VARSIZE_ANY_EXHDR(txt);
    char buf[IP6R_STRING_MAX];

    if (tlen < sizeof(buf))
    {
        memcpy(buf, VARDATA_ANY(txt), tlen);
        buf[tlen] = 0;

		PG_RETURN_DATUM(DirectFunctionCall1(iprange_in, CStringGetDatum(buf)));
    }

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IPR value in text")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(iprange_cast_from_cidr);
Datum
iprange_cast_from_cidr(PG_FUNCTION_ARGS)
{
    inet *inetptr = PG_GETARG_INET_P(0);
    inet_struct *in = INET_STRUCT_DATA(inetptr);
	unsigned char *p = in->ipaddr;
	IPR ipr;

    if (in->bits <= ipr_af_maxbits(in->family))
	{
		switch (in->family)
		{
			case PGSQL_AF_INET:
			{
				IP4 ip = (p[0] << 24)|(p[1] << 16)|(p[2] << 8)|p[3];
				if (ip4r_from_cidr(ip, in->bits, &ipr.ip4r))
					PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET,&ipr));
			}
			break;

			case PGSQL_AF_INET6:
			{
				IP6 ip;
				ip.bits[0] = (((uint64)p[0] << 56)
							  | ((uint64)p[1] << 48)
							  | ((uint64)p[2] << 40)
							  | ((uint64)p[3] << 32)
							  | ((uint64)p[4] << 24)
							  | ((uint64)p[5] << 16)
							  | ((uint64)p[6] << 8)
							  | p[7]);
				ip.bits[1] = (((uint64)p[8] << 56)
							  | ((uint64)p[9] << 48)
							  | ((uint64)p[10] << 40)
							  | ((uint64)p[11] << 32)
							  | ((uint64)p[12] << 24)
							  | ((uint64)p[13] << 16)
							  | ((uint64)p[14] << 8)
							  | p[15]);
				if (ip6r_from_cidr(&ip, in->bits, &ipr.ip6r))
					PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6,&ipr));
			}
			break;
		}
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid CIDR value for conversion to IPR")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(iprange_cast_to_cidr);
Datum
iprange_cast_to_cidr(PG_FUNCTION_ARGS)
{
    IPR_P iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(iprp, &ipr);
    inet *res;
    inet_struct *in;
	unsigned bits;

	switch (af)
	{
		case 0:
			PG_RETURN_NULL();

		case PGSQL_AF_INET:
			bits = masklen(ipr.ip4r.lower, ipr.ip4r.upper);
			if (bits > ipr_af_maxbits(PGSQL_AF_INET))
				PG_RETURN_NULL();

			res = palloc0(VARHDRSZ + sizeof(inet_struct));
			SET_VARSIZE(res, VARHDRSZ + offsetof(inet_struct, ipaddr) + 4);

			in = ((inet_struct *)VARDATA(res));
			in->bits = bits;
			in->family = PGSQL_AF_INET;
			{
				unsigned char *p = in->ipaddr;
				IP4 ip = ipr.ip4r.lower;
				p[0] = (ip >> 24);
				p[1] = (ip >> 16);
				p[2] = (ip >>  8);
				p[3] = (ip      );
			}

			PG_RETURN_INET_P(res);

		case PGSQL_AF_INET6:
			bits = masklen6(&ipr.ip6r.lower, &ipr.ip6r.upper);
			if (bits > ipr_af_maxbits(PGSQL_AF_INET6))
				PG_RETURN_NULL();

			res = palloc0(VARHDRSZ + sizeof(inet_struct));
			SET_VARSIZE(res, VARHDRSZ + offsetof(inet_struct, ipaddr) + 16);

			in = ((inet_struct *)VARDATA(res));
			in->bits = bits;
			in->family = PGSQL_AF_INET6;
			{
				unsigned char *p = in->ipaddr;
				uint64 b = ipr.ip6r.lower.bits[0];
				p[0] = (b >> 56);
				p[1] = (b >> 48);
				p[2] = (b >> 40);
				p[3] = (b >> 32);
				p[4] = (b >> 24);
				p[5] = (b >> 16);
				p[6] = (b >> 8);
				p[7] = (b);
				b = ipr.ip6r.lower.bits[1];
				p[8] = (b >> 56);
				p[9] = (b >> 48);
				p[10] = (b >> 40);
				p[11] = (b >> 32);
				p[12] = (b >> 24);
				p[13] = (b >> 16);
				p[14] = (b >> 8);
				p[15] = (b);
			}

			PG_RETURN_INET_P(res);

		default:
			iprange_internal_error();
	}
}

PG_FUNCTION_INFO_V1(iprange_cast_from_ip4);
Datum
iprange_cast_from_ip4(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
	IPR res;

    if (ip4r_from_inet(ip, 32, &res.ip4r))
        PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET, &res));

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP4 value for conversion to IPR (shouldn't be possible)")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(iprange_cast_from_ip6);
Datum
iprange_cast_from_ip6(PG_FUNCTION_ARGS)
{
    IP6 *ip = PG_GETARG_IP6_P(0);
	IPR res;

    if (ip6r_from_inet(ip, 128, &res.ip6r))
        PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6, &res));

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP6 value for conversion to IPR (shouldn't be possible)")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(iprange_cast_from_ipaddr);
Datum
iprange_cast_from_ipaddr(PG_FUNCTION_ARGS)
{
    IP_P ipp = PG_GETARG_IP_P(0);
	IP ip;
	IPR res;
	int af = ip_unpack(ipp, &ip);

	switch (af)
	{
		case PGSQL_AF_INET:
			if (ip4r_from_inet(ip.ip4, 32, &res.ip4r))
				PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET, &res));
			break;

		case PGSQL_AF_INET6:
			if (ip6r_from_inet(&ip.ip6, 128, &res.ip6r))
				PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6, &res));
			break;
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP6 value for conversion to IPR (shouldn't be possible)")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(iprange_cast_from_ip4r);
Datum
iprange_cast_from_ip4r(PG_FUNCTION_ARGS)
{
    IP4R *ipr = PG_GETARG_IP4R_P(0);
	IPR res;

	res.ip4r = *ipr;
	PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET, &res));
}

PG_FUNCTION_INFO_V1(iprange_cast_from_ip6r);
Datum
iprange_cast_from_ip6r(PG_FUNCTION_ARGS)
{
    IP6R *ipr = PG_GETARG_IP6R_P(0);
	IPR res;

	res.ip6r = *ipr;
	PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6, &res));
}

PG_FUNCTION_INFO_V1(iprange_cast_to_ip4r);
Datum
iprange_cast_to_ip4r(PG_FUNCTION_ARGS)
{
    IPR_P iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(iprp,&ipr);
	IP4R *res;

	if (af != PGSQL_AF_INET)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid IPR value for conversion to IP4R")));

	res = palloc(sizeof(IP4R));
	*res = ipr.ip4r;

	PG_RETURN_IP4R_P(res);
}

PG_FUNCTION_INFO_V1(iprange_cast_to_ip6r);
Datum
iprange_cast_to_ip6r(PG_FUNCTION_ARGS)
{
    IPR_P iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(iprp,&ipr);
	IP6R *res;

	if (af != PGSQL_AF_INET6)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid IPR value for conversion to IP6R")));

	res = palloc(sizeof(IP6R));
	*res = ipr.ip6r;

	PG_RETURN_IP6R_P(res);
}

PG_FUNCTION_INFO_V1(iprange_cast_to_bit);
Datum
iprange_cast_to_bit(PG_FUNCTION_ARGS)
{
    IPR_P iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(iprp, &ipr);
	unsigned bits;
	VarBit *res;
	int len;
	unsigned char buf[16];

	switch (af)
	{
		case 0:
			PG_RETURN_NULL();

		case PGSQL_AF_INET:
			bits = masklen(ipr.ip4r.lower, ipr.ip4r.upper);
			if (bits > ipr_af_maxbits(PGSQL_AF_INET))
				PG_RETURN_NULL();

			{
				IP4 ip = ipr.ip4r.lower;
				buf[0] = (ip >> 24);
				buf[1] = (ip >> 16);
				buf[2] = (ip >>  8);
				buf[3] = (ip      );
			}
			break;

		case PGSQL_AF_INET6:
			bits = masklen6(&ipr.ip6r.lower, &ipr.ip6r.upper);
			if (bits > ipr_af_maxbits(PGSQL_AF_INET6))
				PG_RETURN_NULL();

			ip6_serialize(&ipr.ip6r.lower, buf);
			break;

		default:
			iprange_internal_error();
	}

	len = VARBITTOTALLEN(bits);
	res = palloc0(len);
	SET_VARSIZE(res, len);
	VARBITLEN(res) = bits;

	memcpy(VARBITS(res), buf, VARBITBYTES(res));
	PG_RETURN_VARBIT_P(res);
}


static
Datum
iprange_from_ipaddrs_internal(int af, IP4 a4, IP4 b4, IP6 *a6, IP6 *b6)
{
	IPR res;

	switch (af)
	{
		case PGSQL_AF_INET:
			if (ip4_lessthan(a4,b4))
				res.ip4r.lower = a4, res.ip4r.upper = b4;
			else
				res.ip4r.lower = b4, res.ip4r.upper = a4;
			PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET,&res));

		case PGSQL_AF_INET6:
			if (ip6_lessthan(a6,b6))
				res.ip6r.lower = *a6, res.ip6r.upper = *b6;
			else
				res.ip6r.lower = *b6, res.ip6r.upper = *a6;
			PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6,&res));

		default:
			iprange_internal_error();
	}
}

PG_FUNCTION_INFO_V1(iprange_from_ip4s);
Datum
iprange_from_ip4s(PG_FUNCTION_ARGS)
{
    IP4 a = PG_GETARG_IP4(0);
    IP4 b = PG_GETARG_IP4(1);

	PG_RETURN_DATUM(iprange_from_ipaddrs_internal(PGSQL_AF_INET, a, b, NULL, NULL));
}

PG_FUNCTION_INFO_V1(iprange_from_ip6s);
Datum
iprange_from_ip6s(PG_FUNCTION_ARGS)
{
    IP6 *a = PG_GETARG_IP6_P(0);
    IP6 *b = PG_GETARG_IP6_P(1);

	PG_RETURN_DATUM(iprange_from_ipaddrs_internal(PGSQL_AF_INET6, 0, 0, a, b));
}

PG_FUNCTION_INFO_V1(iprange_from_ipaddrs);
Datum
iprange_from_ipaddrs(PG_FUNCTION_ARGS)
{
    IP_P ap = PG_GETARG_IP_P(0);
    IP_P bp = PG_GETARG_IP_P(1);
	IP a,b;
	int af_a = ip_unpack(ap,&a);
	int af_b = ip_unpack(bp,&b);

	if (af_a != af_b)
		iprange_af_mismatch();

	PG_RETURN_DATUM(iprange_from_ipaddrs_internal(af_a, a.ip4, b.ip4, &a.ip6, &b.ip6));
}


static Datum
iprange_net_prefix_internal(int af, IP4 ip4, IP6 *ip6, int pfxlen)
{
	IPR res;

    if (pfxlen < 0 || pfxlen > ipr_af_maxbits(af))
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("prefix length out of range")));
    }

	switch (af)
	{
		case PGSQL_AF_INET:
			ip4r_from_inet(ip4, (unsigned)pfxlen, &res.ip4r);
			break;
		case PGSQL_AF_INET6:
			ip6r_from_inet(ip6, (unsigned)pfxlen, &res.ip6r);
			break;
		default:
			iprange_internal_error();
	}

	PG_RETURN_IPR_P(ipr_pack(af,&res));
}

PG_FUNCTION_INFO_V1(iprange_net_prefix_ip4);
Datum
iprange_net_prefix_ip4(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    int pfxlen = PG_GETARG_INT32(1);

	PG_RETURN_DATUM(iprange_net_prefix_internal(PGSQL_AF_INET, ip, NULL, pfxlen));
}

PG_FUNCTION_INFO_V1(iprange_net_prefix_ip6);
Datum
iprange_net_prefix_ip6(PG_FUNCTION_ARGS)
{
    IP6 *ip = PG_GETARG_IP6_P(0);
    int pfxlen = PG_GETARG_INT32(1);

	PG_RETURN_DATUM(iprange_net_prefix_internal(PGSQL_AF_INET6, 0, ip, pfxlen));
}

PG_FUNCTION_INFO_V1(iprange_net_prefix);
Datum
iprange_net_prefix(PG_FUNCTION_ARGS)
{
    IP_P ipp = PG_GETARG_IP_P(0);
	IP ip;
    int pfxlen = PG_GETARG_INT32(1);
	int af = ip_unpack(ipp,&ip);

	PG_RETURN_DATUM(iprange_net_prefix_internal(af, ip.ip4, &ip.ip6, pfxlen));
}

static Datum
iprange_net_mask_internal(int af, IP4 ip4, IP6 *ip6, IP4 mask4, IP6 *mask6)
{
	IPR res;

	switch (af)
	{
		case PGSQL_AF_INET:
			if (!ip4_valid_netmask(mask4))
				break;

			res.ip4r.lower = ip4 & mask4;
			res.ip4r.upper = ip4 | ~mask4;

			PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET,&res));

		case PGSQL_AF_INET6:
			if (!ip6_valid_netmask(mask6->bits[0], mask6->bits[1]))
				break;

			res.ip6r.lower.bits[0] = ip6->bits[0] & mask6->bits[0];
			res.ip6r.lower.bits[1] = ip6->bits[1] & mask6->bits[1];
			res.ip6r.upper.bits[0] = ip6->bits[0] | ~(mask6->bits[0]);
			res.ip6r.upper.bits[1] = ip6->bits[1] | ~(mask6->bits[1]);

			PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6,&res));
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("invalid netmask")));
	PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(iprange_net_mask_ip4);
Datum
iprange_net_mask_ip4(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    IP4 mask = PG_GETARG_IP4(1);

	PG_RETURN_DATUM(iprange_net_mask_internal(PGSQL_AF_INET, ip, NULL, mask, NULL));
}

PG_FUNCTION_INFO_V1(iprange_net_mask_ip6);
Datum
iprange_net_mask_ip6(PG_FUNCTION_ARGS)
{
    IP6 *ip = PG_GETARG_IP6_P(0);
    IP6 *mask = PG_GETARG_IP6_P(1);

	PG_RETURN_DATUM(iprange_net_mask_internal(PGSQL_AF_INET6, 0, ip, 0, mask));
}

PG_FUNCTION_INFO_V1(iprange_net_mask);
Datum
iprange_net_mask(PG_FUNCTION_ARGS)
{
    IP_P ipp = PG_GETARG_IP_P(0);
    IP_P maskp = PG_GETARG_IP_P(1);
	IP ip XINIT(IP_INITIALIZER);
	IP mask XINIT(IP_INITIALIZER);
	int af1 = ip_unpack(ipp,&ip);
	int af2 = ip_unpack(maskp,&mask);

	if (af1 != af2)
		iprange_af_mismatch();

	PG_RETURN_DATUM(iprange_net_mask_internal(af1, ip.ip4, &ip.ip6, mask.ip4, &mask.ip6));
}

PG_FUNCTION_INFO_V1(iprange_lower);
Datum
iprange_lower(PG_FUNCTION_ARGS)
{
    IPR_P iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	IP ip;
	int af = ipr_unpack(iprp,&ipr);

	switch (af)
	{
		case 0:
			ip.ip4 = 0;
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET,&ip));

		case PGSQL_AF_INET:
			ip.ip4 = ipr.ip4r.lower;
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET,&ip));

		case PGSQL_AF_INET6:
			ip.ip6 = ipr.ip6r.lower;
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET6,&ip));

		default:
			iprange_internal_error();
	}
}

PG_FUNCTION_INFO_V1(iprange_upper);
Datum
iprange_upper(PG_FUNCTION_ARGS)
{
    IPR_P iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	IP ip;
	int af = ipr_unpack(iprp,&ipr);

	switch (af)
	{
		case 0:
			ip.ip6.bits[0] = ip.ip6.bits[1] = ~(uint64)0;
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET6,&ip));

		case PGSQL_AF_INET:
			ip.ip4 = ipr.ip4r.upper;
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET,&ip));

		case PGSQL_AF_INET6:
			ip.ip6 = ipr.ip6r.upper;
			PG_RETURN_IP_P(ip_pack(PGSQL_AF_INET6,&ip));

		default:
			iprange_internal_error();
	}
}

PG_FUNCTION_INFO_V1(iprange_is_cidr);
Datum
iprange_is_cidr(PG_FUNCTION_ARGS)
{
    IPR_P iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(iprp,&ipr);

	switch (af)
	{
		case 0:
			PG_RETURN_BOOL(false);

		case PGSQL_AF_INET:
			PG_RETURN_BOOL( (masklen(ipr.ip4r.lower,ipr.ip4r.upper) <= 32U) );

		case PGSQL_AF_INET6:
			PG_RETURN_BOOL( (masklen6(&ipr.ip6r.lower,&ipr.ip6r.upper) <= 128U) );

		default:
			iprange_internal_error();
	}
}

PG_FUNCTION_INFO_V1(iprange_family);
Datum
iprange_family(PG_FUNCTION_ARGS)
{
    IPR_P iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(iprp,&ipr);

	switch (af)
	{
		case 0:
			PG_RETURN_NULL();

		case PGSQL_AF_INET:
			PG_RETURN_INT32(4);

		case PGSQL_AF_INET6:
			PG_RETURN_INT32(6);

		default:
			iprange_internal_error();
	}
}

/*
 * Decompose an arbitrary range into CIDRs
 */
PG_FUNCTION_INFO_V1(iprange_cidr_split);
Datum
iprange_cidr_split(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	IPR *val;
	IPR res;
	int af;

	if (SRF_IS_FIRSTCALL())
	{
		IPR_P *in = PG_GETARG_IPR_P(0);

		funcctx = SRF_FIRSTCALL_INIT();
		val = MemoryContextAlloc(funcctx->multi_call_memory_ctx,
								 sizeof(IPR));
		af = ipr_unpack(in, val);
		funcctx->user_fctx = val;
		/*
		 * We abuse max_calls, which is ignored by the api and only exists as
		 * a convenience variable, to store the current address family. But
		 * out of sheer aesthetics (and for debugging purposes) we store a
		 * value that does actually represent a (loose) upper bound on the row
		 * count.
		 */
		funcctx->max_calls = af ? 2*ipr_af_maxbits(af) : 2;
	}

	funcctx = SRF_PERCALL_SETUP();

	val = funcctx->user_fctx;
	if (!val)
		SRF_RETURN_DONE(funcctx);
	Assert(funcctx->call_cntr < funcctx->max_calls);

	switch (funcctx->max_calls)
	{
		case 2:
			/*
			 * We're splitting '-' into '0.0.0.0/0' and '::/0'.
			 */
			if (funcctx->call_cntr == 0)
			{
				res.ip4r.lower = netmask(0);
				res.ip4r.upper = hostmask(0);
				af = PGSQL_AF_INET;
			}
			else
			{
				funcctx->user_fctx = NULL;  /* this is the last row */
				res.ip6r.lower.bits[0] = netmask6_hi(0);
				res.ip6r.lower.bits[1] = netmask6_lo(0);
				res.ip6r.upper.bits[0] = hostmask6_hi(0);
				res.ip6r.upper.bits[1] = hostmask6_lo(0);
				af = PGSQL_AF_INET6;
			}
			break;

		case 2*ipr_af_maxbits(PGSQL_AF_INET):
			if (ip4r_split_cidr(&val->ip4r, &res.ip4r))
				funcctx->user_fctx = NULL;
			af = PGSQL_AF_INET;
			break;

		case 2*ipr_af_maxbits(PGSQL_AF_INET6):
			if (ip6r_split_cidr(&val->ip6r, &res.ip6r))
				funcctx->user_fctx = NULL;
			af = PGSQL_AF_INET6;
			break;

		default:
			iprange_internal_error();
	}

	SRF_RETURN_NEXT(funcctx, IPR_PGetDatum(ipr_pack(af, &res)));
}

/*
 * comparisons and indexing
 */

static int
iprange_cmp_internal(Datum d1, Datum d2)
{
	IPR_P ipp1 = DatumGetIPR_P(d1);
	IPR_P ipp2 = DatumGetIPR_P(d2);
	IPR ipr1 XINIT(IPR_INITIALIZER);
	IPR ipr2 XINIT(IPR_INITIALIZER);
	int af1 = ipr_unpack(ipp1, &ipr1);
	int af2 = ipr_unpack(ipp2, &ipr2);
	int retval = 0;

	if (af1 == af2)
	{
		switch (af1)
		{
			case 0:
				break;

			case PGSQL_AF_INET:
				if (ip4r_lessthan(&ipr1.ip4r,&ipr2.ip4r))
					retval = -1;
				else if (ip4r_lessthan(&ipr2.ip4r,&ipr1.ip4r))
					retval = 1;
				break;

			case PGSQL_AF_INET6:
				if (ip6r_lessthan(&ipr1.ip6r,&ipr2.ip6r))
					retval = -1;
				else if (ip6r_lessthan(&ipr2.ip6r,&ipr1.ip6r))
					retval = 1;
				break;

			default:
				iprange_internal_error();
		}
	}
	else if (af1 < af2)
		retval = -1;
	else
		retval = 1;

	if ((Pointer)ipp1 != DatumGetPointer(d1))
		pfree(ipp1);
	if ((Pointer)ipp2 != DatumGetPointer(d2))
		pfree(ipp2);

    return retval;
}

PG_FUNCTION_INFO_V1(iprange_lt);
Datum
iprange_lt(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_cmp_internal(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1)) < 0 );
}

PG_FUNCTION_INFO_V1(iprange_le);
Datum
iprange_le(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_cmp_internal(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1)) <= 0 );
}

PG_FUNCTION_INFO_V1(iprange_gt);
Datum
iprange_gt(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_cmp_internal(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1)) > 0 );
}

PG_FUNCTION_INFO_V1(iprange_ge);
Datum
iprange_ge(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_cmp_internal(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1)) >= 0 );
}

PG_FUNCTION_INFO_V1(iprange_eq);
Datum
iprange_eq(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_cmp_internal(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1)) == 0 );
}

PG_FUNCTION_INFO_V1(iprange_neq);
Datum
iprange_neq(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_cmp_internal(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1)) != 0 );
}

static bool
iprange_overlaps_internal(Datum d1, Datum d2)
{
	IPR_P ipp1 = DatumGetIPR_P(d1);
	IPR_P ipp2 = DatumGetIPR_P(d2);
	IPR ipr1 XINIT(IPR_INITIALIZER);
	IPR ipr2 XINIT(IPR_INITIALIZER);
	int af1 = ipr_unpack(ipp1, &ipr1);
	int af2 = ipr_unpack(ipp2, &ipr2);
	bool retval;

	if (af1 == af2)
	{
		switch (af1)
		{
			case 0:
				retval = true;
				break;

			case PGSQL_AF_INET:
				retval = ip4r_overlaps_internal(&ipr1.ip4r,&ipr2.ip4r);
				break;

			case PGSQL_AF_INET6:
				retval = ip6r_overlaps_internal(&ipr1.ip6r,&ipr2.ip6r);
				break;

			default:
				iprange_internal_error();
		}
	}
	else
		retval = (af1 == 0) || (af2 == 0);

	if ((Pointer)ipp1 != DatumGetPointer(d1))
		pfree(ipp1);
	if ((Pointer)ipp2 != DatumGetPointer(d2))
		pfree(ipp2);

    return retval;
}

static int
iprange_contains_internal(Datum d1, Datum d2, bool eqval)
{
	IPR_P ipp1 = DatumGetIPR_P(d1);
	IPR_P ipp2 = DatumGetIPR_P(d2);
	IPR ipr1 XINIT(IPR_INITIALIZER);
	IPR ipr2 XINIT(IPR_INITIALIZER);
	int af1 = ipr_unpack(ipp1, &ipr1);
	int af2 = ipr_unpack(ipp2, &ipr2);
	bool retval;

	if (af1 == af2)
	{
		switch (af1)
		{
			case 0:
				retval = eqval;
				break;

			case PGSQL_AF_INET:
				retval = ip4r_contains_internal(&ipr1.ip4r,&ipr2.ip4r,eqval);
				break;

			case PGSQL_AF_INET6:
				retval = ip6r_contains_internal(&ipr1.ip6r,&ipr2.ip6r,eqval);
				break;

			default:
				iprange_internal_error();
		}
	}
	else
		retval = (af1 == 0);

	if ((Pointer)ipp1 != DatumGetPointer(d1))
		pfree(ipp1);
	if ((Pointer)ipp2 != DatumGetPointer(d2))
		pfree(ipp2);

    return retval;
}

static int
iprange_contains_ip_internal(Datum d1, int af2, IP4 ip4, IP6 *ip6)
{
	IPR_P ipp1 = DatumGetIPR_P(d1);
	IPR ipr1 XINIT(IPR_INITIALIZER);
	int af1 = ipr_unpack(ipp1, &ipr1);
	bool retval;

	if (af1 == af2)
	{
		switch (af1)
		{
			case 0:
				retval = true;
				break;

			case PGSQL_AF_INET:
				retval = ip4_contains_internal(&ipr1.ip4r,ip4);
				break;

			case PGSQL_AF_INET6:
				retval = ip6_contains_internal(&ipr1.ip6r,ip6);
				break;

			default:
				iprange_internal_error();
		}
	}
	else
		retval = (af1 == 0);

	if ((Pointer)ipp1 != DatumGetPointer(d1))
		pfree(ipp1);

    return retval;
}

PG_FUNCTION_INFO_V1(iprange_overlaps);
Datum
iprange_overlaps(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_overlaps_internal(PG_GETARG_DATUM(0),
										  PG_GETARG_DATUM(1)) );
}

PG_FUNCTION_INFO_V1(iprange_contains);
Datum
iprange_contains(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_contains_internal(PG_GETARG_DATUM(0),
										  PG_GETARG_DATUM(1),
										  true) );
}

PG_FUNCTION_INFO_V1(iprange_contains_strict);
Datum
iprange_contains_strict(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_contains_internal(PG_GETARG_DATUM(0),
										  PG_GETARG_DATUM(1),
										  false) );
}

PG_FUNCTION_INFO_V1(iprange_contained_by);
Datum
iprange_contained_by(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_contains_internal(PG_GETARG_DATUM(1),
										  PG_GETARG_DATUM(0),
										  true) );
}

PG_FUNCTION_INFO_V1(iprange_contained_by_strict);
Datum
iprange_contained_by_strict(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( iprange_contains_internal(PG_GETARG_DATUM(1),
										  PG_GETARG_DATUM(0),
										  false) );
}

PG_FUNCTION_INFO_V1(iprange_contains_ip);
Datum
iprange_contains_ip(PG_FUNCTION_ARGS)
{
	IP_P ipp = PG_GETARG_IP_P(1);
	IP ip XINIT(IP_INITIALIZER);
	int af = ip_unpack(ipp,&ip);
	bool retval = iprange_contains_ip_internal(PG_GETARG_DATUM(0), af, ip.ip4, &ip.ip6);

	PG_FREE_IF_COPY(ipp,1);
	PG_RETURN_BOOL(retval);
}

PG_FUNCTION_INFO_V1(iprange_contains_ip4);
Datum
iprange_contains_ip4(PG_FUNCTION_ARGS)
{
	IP4 ip = PG_GETARG_IP4(1);
	PG_RETURN_BOOL(iprange_contains_ip_internal(PG_GETARG_DATUM(0), PGSQL_AF_INET, ip, NULL));
}

PG_FUNCTION_INFO_V1(iprange_contains_ip6);
Datum
iprange_contains_ip6(PG_FUNCTION_ARGS)
{
	IP6 *ip = PG_GETARG_IP6_P(1);
	PG_RETURN_BOOL(iprange_contains_ip_internal(PG_GETARG_DATUM(0), PGSQL_AF_INET6, 0, ip));
}

PG_FUNCTION_INFO_V1(iprange_ip6_contained_by);
Datum
iprange_ip6_contained_by(PG_FUNCTION_ARGS)
{
	IP6 *ip = PG_GETARG_IP6_P(0);
    PG_RETURN_BOOL( iprange_contains_ip_internal(PG_GETARG_DATUM(1), PGSQL_AF_INET6, 0, ip) );
}

PG_FUNCTION_INFO_V1(iprange_ip4_contained_by);
Datum
iprange_ip4_contained_by(PG_FUNCTION_ARGS)
{
	IP4 ip = PG_GETARG_IP4(0);
    PG_RETURN_BOOL( iprange_contains_ip_internal(PG_GETARG_DATUM(1), PGSQL_AF_INET, ip, NULL) );
}

PG_FUNCTION_INFO_V1(iprange_ip_contained_by);
Datum
iprange_ip_contained_by(PG_FUNCTION_ARGS)
{
	IP_P ipp = PG_GETARG_IP_P(0);
	IP ip XINIT(IP_INITIALIZER);
	int af = ip_unpack(ipp,&ip);
	bool retval = iprange_contains_ip_internal(PG_GETARG_DATUM(1), af, ip.ip4, &ip.ip6);

	PG_FREE_IF_COPY(ipp,0);
	PG_RETURN_BOOL(retval);
}

PG_FUNCTION_INFO_V1(iprange_union);
Datum
iprange_union(PG_FUNCTION_ARGS)
{
	IPR_P ipp1 = PG_GETARG_IPR_P(0);
	IPR_P ipp2 = PG_GETARG_IPR_P(1);
	IPR ipr1;
	IPR ipr2;
	int af1 = ipr_unpack(ipp1, &ipr1);
	int af2 = ipr_unpack(ipp2, &ipr2);
	IPR res;

	if (af1 == af2)
	{
		switch (af1)
		{
			case 0:
				PG_RETURN_IPR_P(ipr_pack(0,NULL));

			case PGSQL_AF_INET:
				ip4r_union_internal(&ipr1.ip4r,&ipr2.ip4r,&res.ip4r);
				PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET,&res));

			case PGSQL_AF_INET6:
				ip6r_union_internal(&ipr1.ip6r,&ipr2.ip6r,&res.ip6r);
				PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6,&res));

			default:
				iprange_internal_error();
		}
	}
	else
		PG_RETURN_IPR_P(ipr_pack(0,NULL));
}

PG_FUNCTION_INFO_V1(iprange_inter);
Datum
iprange_inter(PG_FUNCTION_ARGS)
{
	IPR_P ipp1 = PG_GETARG_IPR_P(0);
	IPR_P ipp2 = PG_GETARG_IPR_P(1);
	IPR ipr1;
	IPR ipr2;
	int af1 = ipr_unpack(ipp1, &ipr1);
	int af2 = ipr_unpack(ipp2, &ipr2);
	IPR res;

	if (af1 == af2)
	{
		switch (af1)
		{
			case 0:
				PG_RETURN_IPR_P(ipr_pack(0,NULL));

			case PGSQL_AF_INET:
				if (ip4r_inter_internal(&ipr1.ip4r,&ipr2.ip4r,&res.ip4r))
					PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET,&res));
				break;

			case PGSQL_AF_INET6:
				if (ip6r_inter_internal(&ipr1.ip6r,&ipr2.ip6r,&res.ip6r))
					PG_RETURN_IPR_P(ipr_pack(PGSQL_AF_INET6,&res));
				break;

			default:
				iprange_internal_error();
		}
	}
	else if (af1 == 0)
		PG_RETURN_IPR_P(ipr_pack(af2,&ipr2));
	else if (af2 == 0)
		PG_RETURN_IPR_P(ipr_pack(af1,&ipr1));

    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(iprange_size);
Datum
iprange_size(PG_FUNCTION_ARGS)
{
	IPR_P ipp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(ipp, &ipr);

	switch (af)
	{
		case 0:
			PG_RETURN_FLOAT8(ldexp(1.0, 129));

		case PGSQL_AF_INET:
			PG_RETURN_FLOAT8(ip4r_metric(&ipr.ip4r));

		case PGSQL_AF_INET6:
			PG_RETURN_FLOAT8(ip6r_metric(&ipr.ip6r));

		default:
			iprange_internal_error();
	}
}

PG_FUNCTION_INFO_V1(iprange_size_exact);
Datum
iprange_size_exact(PG_FUNCTION_ARGS)
{
	IPR_P ipp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(ipp, &ipr);
	Datum u,l,d,s;

	switch (af)
	{
		case 0:
			s = DirectFunctionCall3(numeric_in, CStringGetDatum("680564733841876926926749214863536422912"), 0, Int32GetDatum(-1));
			PG_RETURN_DATUM(s);

		case PGSQL_AF_INET:
			l = DirectFunctionCall1(ip4_cast_to_numeric, IP4GetDatum(ipr.ip4r.lower));
			u = DirectFunctionCall1(ip4_cast_to_numeric, IP4GetDatum(ipr.ip4r.upper));
			break;

		case PGSQL_AF_INET6:
			l = DirectFunctionCall1(ip6_cast_to_numeric, IP6PGetDatum(&ipr.ip6r.lower));
			u = DirectFunctionCall1(ip6_cast_to_numeric, IP6PGetDatum(&ipr.ip6r.upper));
			break;

		default:
			iprange_internal_error();
	}

	d = DirectFunctionCall2(numeric_sub, u, l);
	s = DirectFunctionCall1(numeric_inc, d);
	PG_RETURN_DATUM(s);
}

PG_FUNCTION_INFO_V1(iprange_prefixlen);
Datum
iprange_prefixlen(PG_FUNCTION_ARGS)
{
    IPR_P iprp = PG_GETARG_IPR_P(0);
	IPR ipr;
	int af = ipr_unpack(iprp,&ipr);
	unsigned len = ~0;
	unsigned maxbits = 0;

	if (af == PGSQL_AF_INET)
		maxbits = 32, len = masklen(ipr.ip4r.lower, ipr.ip4r.upper);
	else if (af == PGSQL_AF_INET6)
		maxbits = 128, len = masklen6(&ipr.ip6r.lower, &ipr.ip6r.upper);

	if (len <= maxbits)
		PG_RETURN_INT32((int32) len);

	PG_RETURN_NULL();
}


/*****************************************************************************
 *                                                 Btree functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(iprange_cmp);
Datum
iprange_cmp(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32( iprange_cmp_internal(PG_GETARG_DATUM(0), PG_GETARG_DATUM(1)) );
}


/*****************************************************************************
 *                                                 GiST functions
 *****************************************************************************/

/*
** GiST support methods
*/

Datum gipr_consistent(PG_FUNCTION_ARGS);
Datum gipr_compress(PG_FUNCTION_ARGS);
Datum gipr_decompress(PG_FUNCTION_ARGS);
Datum gipr_penalty(PG_FUNCTION_ARGS);
Datum gipr_picksplit(PG_FUNCTION_ARGS);
Datum gipr_union(PG_FUNCTION_ARGS);
Datum gipr_same(PG_FUNCTION_ARGS);
Datum gipr_fetch(PG_FUNCTION_ARGS);

typedef struct {
	int32 vl_len_;
	int32 af;
	IPR ipr;
} IPR_KEY;

static bool gipr_leaf_consistent(IPR_KEY *key, IPR_P query, StrategyNumber strategy);
static bool gipr_internal_consistent(IPR_KEY *key, IPR_P query, StrategyNumber strategy);


/*
 * compress is passed a GISTENTRY* containing a leaf or nonleaf key, and is
 * expected to return either the same entry or a new one containing the data to
 * be actually written to the index tuple. The key is a leaf key if and only if
 * it came from outside GiST (via insert or bulkinsert).
 */

PG_FUNCTION_INFO_V1(gipr_compress);
Datum
gipr_compress(PG_FUNCTION_ARGS)
{
    GISTENTRY *entry = (GISTENTRY *) PG_GETARG_POINTER(0);
	GISTENTRY *retval = entry;

	if (!entry->leafkey)
	{
		IPR_KEY *key = (IPR_KEY *) DatumGetPointer(entry->key);

		retval = palloc(sizeof(GISTENTRY));

		Assert(!VARATT_IS_EXTENDED(key) && VARSIZE(key) == sizeof(IPR_KEY));
		Assert(key->af == 0 || key->af == PGSQL_AF_INET || key->af == PGSQL_AF_INET6);

		gistentryinit(*retval, PointerGetDatum(ipr_pack(key->af, &key->ipr)),
					  entry->rel, entry->page,
					  entry->offset, false);
	}

	PG_RETURN_POINTER(retval);
}

PG_FUNCTION_INFO_V1(gipr_decompress);
Datum
gipr_decompress(PG_FUNCTION_ARGS)
{
    GISTENTRY *entry = (GISTENTRY *) PG_GETARG_POINTER(0);
	GISTENTRY *retval = palloc(sizeof(GISTENTRY));
	IPR_KEY *key = palloc(sizeof(IPR_KEY));

	SET_VARSIZE(key, sizeof(IPR_KEY));
	key->af = ipr_unpack((IPR_P) DatumGetPointer(entry->key), &key->ipr);

	gistentryinit(*retval, PointerGetDatum(key),
				  entry->rel, entry->page,
				  entry->offset, false);

    PG_RETURN_POINTER(retval);
}

PG_FUNCTION_INFO_V1(gipr_fetch);
Datum
gipr_fetch(PG_FUNCTION_ARGS)
{
    PG_RETURN_POINTER(PG_GETARG_POINTER(0));
}


/*
** The GiST Consistent method for IP ranges
** Should return false if for all data items x below entry,
** the predicate x op query == false, where op is the oper
** corresponding to strategy in the pg_amop table.
*/

PG_FUNCTION_INFO_V1(gipr_consistent);
Datum
gipr_consistent(PG_FUNCTION_ARGS)
{
    GISTENTRY *entry = (GISTENTRY *) PG_GETARG_POINTER(0);
    IPR_P query = (IPR_P) PG_GETARG_POINTER(1);
    StrategyNumber strategy = (StrategyNumber) PG_GETARG_UINT16(2);
    bool *recheck = (bool *) PG_GETARG_POINTER(4);
    IPR_KEY *key = (IPR_KEY *) DatumGetPointer(entry->key);
    bool retval;

    /* recheck is never needed with this type */
    if (recheck)
        *recheck = false;

    /*
     * * if entry is not leaf, use gipr_internal_consistent, * else use
     * gipr_leaf_consistent
     */

    if (GIST_LEAF(entry))
        retval = gipr_leaf_consistent(key, query, strategy);
    else
        retval = gipr_internal_consistent(key, query, strategy);

    PG_RETURN_BOOL(retval);
}

/*
** The GiST Union method for IP ranges
** returns the minimal bounding IPR that encloses all the entries in entryvec
*/

static void
gipr_union_internal_1(IPR_KEY *out, IPR_KEY *tmp)
{
	if (out->af != tmp->af)
		out->af = 0;

	switch (out->af)
	{
		case 0:
			break;

		case PGSQL_AF_INET:
		{
			if (ip4_lessthan(tmp->ipr.ip4r.lower,out->ipr.ip4r.lower))
				out->ipr.ip4r.lower = tmp->ipr.ip4r.lower;
			if (ip4_lessthan(out->ipr.ip4r.upper,tmp->ipr.ip4r.upper))
				out->ipr.ip4r.upper = tmp->ipr.ip4r.upper;
			break;
		}

		case PGSQL_AF_INET6:
		{
			if (ip6_lessthan(&tmp->ipr.ip6r.lower,&out->ipr.ip6r.lower))
				out->ipr.ip6r.lower = tmp->ipr.ip6r.lower;
			if (ip6_lessthan(&out->ipr.ip6r.upper,&tmp->ipr.ip6r.upper))
				out->ipr.ip6r.upper = tmp->ipr.ip6r.upper;
			break;
		}

		default:
			iprange_internal_error();
	}
}

static void
gipr_union_internal(IPR_KEY *out, bool *allequalp, bool *afequalp,
						GISTENTRY *ent, int numranges)
{
	int i;
	bool allequal = true;
	bool afequal = true;
	IPR_KEY *tmp;

    tmp = (IPR_KEY *) DatumGetPointer(ent[0].key);
	*out = *tmp;

	for (i = 1; out->af != 0 && i < numranges; ++i)
	{
		tmp = (IPR_KEY *) DatumGetPointer(ent[i].key);
		if (tmp->af != out->af)
		{
			out->af = 0;
			afequal = allequal = false;
		}
	}

	switch (out->af)
	{
		case 0:
			break;

		case PGSQL_AF_INET:
		{
			tmp = (IPR_KEY *) DatumGetPointer(ent[0].key);
			out->ipr.ip4r = tmp->ipr.ip4r;

			for (i = 1; i < numranges; i++)
			{
				tmp = (IPR_KEY *) DatumGetPointer(ent[i].key);

				if (allequal && !ip4r_equal(&tmp->ipr.ip4r, &out->ipr.ip4r))
					allequal = false;

				if (ip4_lessthan(tmp->ipr.ip4r.lower,out->ipr.ip4r.lower))
					out->ipr.ip4r.lower = tmp->ipr.ip4r.lower;
				if (ip4_lessthan(out->ipr.ip4r.upper,tmp->ipr.ip4r.upper))
					out->ipr.ip4r.upper = tmp->ipr.ip4r.upper;
			}
			break;
		}

		case PGSQL_AF_INET6:
		{
			tmp = (IPR_KEY *) DatumGetPointer(ent[0].key);
			out->ipr.ip4r = tmp->ipr.ip4r;

			for (i = 1; i < numranges; i++)
			{
				tmp = (IPR_KEY *) DatumGetPointer(ent[i].key);

				if (allequal && !ip6r_equal(&tmp->ipr.ip6r, &out->ipr.ip6r))
					allequal = false;

				if (ip6_lessthan(&tmp->ipr.ip6r.lower,&out->ipr.ip6r.lower))
					out->ipr.ip6r.lower = tmp->ipr.ip6r.lower;
				if (ip6_lessthan(&out->ipr.ip6r.upper,&tmp->ipr.ip6r.upper))
					out->ipr.ip6r.upper = tmp->ipr.ip6r.upper;
			}
			break;
		}

		default:
			iprange_internal_error();
	}

	if (afequalp)
		*afequalp = afequal;
	if (allequalp)
		*allequalp = allequal;
}


PG_FUNCTION_INFO_V1(gipr_union);
Datum
gipr_union(PG_FUNCTION_ARGS)
{
    GistEntryVector *entryvec = (GistEntryVector *) PG_GETARG_POINTER(0);
    int *sizep = (int *) PG_GETARG_POINTER(1);
    GISTENTRY *ent = GISTENTRYVEC(entryvec);
    int numranges;
	IPR_KEY *out = palloc(sizeof(IPR_KEY));

#ifdef GIST_DEBUG
    fprintf(stderr, "union\n");
#endif

    numranges = GISTENTRYCOUNT(entryvec);

	gipr_union_internal(out, NULL, NULL, ent, numranges);

	*sizep = sizeof(IPR_KEY);

    PG_RETURN_POINTER(out);
}


/*
** The GiST Penalty method for IP ranges
** As in the R-tree paper, we use change in area as our penalty metric
*/
PG_FUNCTION_INFO_V1(gipr_penalty);
Datum
gipr_penalty(PG_FUNCTION_ARGS)
{
    GISTENTRY *origentry = (GISTENTRY *) PG_GETARG_POINTER(0);
    GISTENTRY *newentry = (GISTENTRY *) PG_GETARG_POINTER(1);
    float *result = (float *) PG_GETARG_POINTER(2);
    IPR_KEY *key = (IPR_KEY *) DatumGetPointer(origentry->key);
    IPR_KEY *newkey = (IPR_KEY *) DatumGetPointer(newentry->key);
    IP4R ud4;
    IP6R ud6;
	double tmp = 0.0;

	if (key->af != newkey->af)
	{
		if (key->af != 0 && newkey->af != 0)
			tmp = 1e10;
	}
	else
	{
		switch (key->af)
		{
			case 0:
				break;

				/*
				 * rather than subtract the sizes, which might lose
				 * due to rounding errors in v6, we calculate the
				 * actual number of addresses added to the range.
				 */

			case PGSQL_AF_INET:
				if (newkey->ipr.ip4r.lower < key->ipr.ip4r.lower)
				{
					ud4.lower = newkey->ipr.ip4r.lower;
					ud4.upper = key->ipr.ip4r.lower - 1;
					tmp = ip4r_metric(&ud4);
				}
				if (key->ipr.ip4r.upper < newkey->ipr.ip4r.upper)
				{
					ud4.lower = key->ipr.ip4r.upper;
					ud4.upper = newkey->ipr.ip4r.upper - 1;
					tmp += ip4r_metric(&ud4);
				}
				break;

			case PGSQL_AF_INET6:
				if (ip6_lessthan(&newkey->ipr.ip6r.lower,&key->ipr.ip6r.lower))
				{
					ud6.lower = newkey->ipr.ip6r.lower;
					ud6.upper = key->ipr.ip6r.lower;
					ip6_sub_int(&ud6.upper,1,&ud6.upper);
					tmp = ip6r_metric(&ud6);
				}
				if (ip6_lessthan(&key->ipr.ip6r.upper,&newkey->ipr.ip6r.upper))
				{
					ud6.lower = key->ipr.ip6r.upper;
					ud6.upper = newkey->ipr.ip6r.upper;
					ip6_sub_int(&ud6.upper,1,&ud6.upper);
					tmp += ip6r_metric(&ud6);
				}

				/*
				 * we want to scale the result a bit. For one thing, the gist code implicitly
				 * assigns a penalty of 1e10 for a union of null and non-null values, and we
				 * want to keep our values less than that. For another, the penalty is sometimes
				 * summed across columns of a multi-column index, and we don't want our huge
				 * metrics (>2^80) to completely swamp anything else.
				 *
				 * So, we scale as the fourth power of the log2 of the computed penalty, which
				 * gives us a range 0 - 268435456.
				 */

				tmp = pow(log(tmp+1) / log(2), 4);
				break;

			default:
				iprange_internal_error();
		}
	}

	*result = (float) tmp;

#ifdef GIST_DEBUG
    fprintf(stderr, "penalty\n");
    fprintf(stderr, "\t%g\n", *result);
#endif

    PG_RETURN_POINTER(result);
}


/* Helper functions for picksplit. We might need to sort a list of
 * ranges by size; these are for that. We don't ever need to sort
 * mixed address families though.
 */

struct gipr_sort
{
    IPR_KEY *key;
    OffsetNumber pos;
};

static int
gipr_sort_compare_v4(const void *av, const void *bv)
{
	IPR_KEY *a = ((struct gipr_sort *)av)->key;
	IPR_KEY *b = ((struct gipr_sort *)bv)->key;
	double sa = ip4r_metric(&a->ipr.ip4r);
	double sb = ip4r_metric(&b->ipr.ip4r);
	return (sa > sb) ? 1 : ((sa == sb) ? 0 : -1);
}

static int
gipr_sort_compare_v6(const void *av, const void *bv)
{
	IPR_KEY *a = ((struct gipr_sort *)av)->key;
	IPR_KEY *b = ((struct gipr_sort *)bv)->key;
	double sa = ip6r_metric(&a->ipr.ip6r);
	double sb = ip6r_metric(&b->ipr.ip6r);
	return (sa > sb) ? 1 : ((sa == sb) ? 0 : -1);
}

/*
** The GiST PickSplit method for IP ranges
** This is a linear-time algorithm based on a left/right split,
** based on the box functions in rtree_gist simplified to one
** dimension
*/
PG_FUNCTION_INFO_V1(gipr_picksplit);
Datum
gipr_picksplit(PG_FUNCTION_ARGS)
{
    GistEntryVector *entryvec = (GistEntryVector *) PG_GETARG_POINTER(0);
    GIST_SPLITVEC *v = (GIST_SPLITVEC *) PG_GETARG_POINTER(1);
    GISTENTRY *ent = GISTENTRYVEC(entryvec);
    OffsetNumber i;
    int nbytes;
    OffsetNumber maxoff;
    OffsetNumber *listL;
    OffsetNumber *listR;
    bool allisequal = true;
    bool allafequal = true;
    IPR_KEY pageunion;
    IPR_KEY *cur;
    IPR_KEY *unionL;
    IPR_KEY *unionR;
    int posL = 0;
    int posR = 0;

    posL = posR = 0;
    maxoff = GISTENTRYCOUNT(entryvec) - 1;

	gipr_union_internal(&pageunion, &allisequal, &allafequal,
						&ent[FirstOffsetNumber], maxoff);

    nbytes = (maxoff + 2) * sizeof(OffsetNumber);
    listL = (OffsetNumber *) palloc(nbytes);
    listR = (OffsetNumber *) palloc(nbytes);
    unionL = palloc(sizeof(IPR_KEY));
    unionR = palloc(sizeof(IPR_KEY));
    v->spl_ldatum = PointerGetDatum(unionL);
    v->spl_rdatum = PointerGetDatum(unionR);
    v->spl_left = listL;
    v->spl_right = listR;

    if (allisequal)
    {
		OffsetNumber split_at = FirstOffsetNumber + (maxoff - FirstOffsetNumber + 1)/2;
		v->spl_nleft = v->spl_nright = 0;
		*unionL = pageunion;
		*unionR = pageunion;

		for (i = FirstOffsetNumber; i < split_at; i = OffsetNumberNext(i))
			v->spl_left[v->spl_nleft++] = i;
		for (; i <= maxoff; i = OffsetNumberNext(i))
			v->spl_right[v->spl_nright++] = i;

		PG_RETURN_POINTER(v);
    }

	/*
	 * if we have a mix of address families present, then we split by AF regardless
	 * of all other factors, since the penalty for mixing them is so high. If there's
	 * at least one universal range, we split those into the right page and leave
	 * everything else in the left page; otherwise, we split ivp6 into the right page.
	 * We accept a bad split ratio here in the interests of keeping AFs separate.
	 */

#define ADDLIST( list_, u_, pos_, num_ ) do { \
        if ( pos_ ) { \
			gipr_union_internal_1(u_, cur); \
        } else { \
                *(u_) = *(cur); \
        } \
        (list_)[(pos_)++] = (num_); \
} while(0)

	if (!allafequal)
	{
		int right_af = PGSQL_AF_INET6;

		for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
			if ( ((IPR_KEY *)DatumGetPointer(ent[i].key))->af == 0)
				break;

		if (i <= maxoff)
			right_af = 0;

		for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
		{
			cur = (IPR_KEY *)DatumGetPointer(ent[i].key);
			if (cur->af != right_af)
				ADDLIST(listL, unionL, posL, i);
			else
				ADDLIST(listR, unionR, posR, i);
		}
	}
	else if (pageunion.af == PGSQL_AF_INET)
	{
		for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
		{
			cur = (IPR_KEY *) DatumGetPointer(ent[i].key);
			if ((cur->ipr.ip4r.upper - pageunion.ipr.ip4r.lower)
				< (pageunion.ipr.ip4r.upper - cur->ipr.ip4r.lower))
				ADDLIST(listL, unionL, posL, i);
			else
				ADDLIST(listR, unionR, posR, i);
		}
	}
	else
	{
		for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
		{
			IP6 diff1;
			IP6 diff2;

			cur = (IPR_KEY *) DatumGetPointer(ent[i].key);
			ip6_sub(&cur->ipr.ip6r.upper, &pageunion.ipr.ip6r.lower, &diff1);
			ip6_sub(&pageunion.ipr.ip6r.upper, &cur->ipr.ip6r.lower, &diff2);
			if (ip6_lessthan(&diff1,&diff2))
				ADDLIST(listL, unionL, posL, i);
			else
				ADDLIST(listR, unionR, posR, i);
		}
	}

    /* bad disposition, sort by ascending size and resplit */
    if (posR == 0 || posL == 0)
    {
        struct gipr_sort *arr = (struct gipr_sort *)
            palloc(sizeof(struct gipr_sort) * (maxoff + FirstOffsetNumber));

		Assert(allafequal);

        for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
        {
            arr[i].key = (IPR_KEY *) DatumGetPointer(ent[i].key);
            arr[i].pos = i;
        }

        qsort(arr + FirstOffsetNumber,
              maxoff - FirstOffsetNumber + 1,
              sizeof(struct gipr_sort),
			  (pageunion.af == PGSQL_AF_INET6) ? gipr_sort_compare_v6 : gipr_sort_compare_v4);

        posL = posR = 0;

		for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
		{
			IP6 diff1;
			IP6 diff2;

			cur = arr[i].key;
			if (pageunion.af == PGSQL_AF_INET)
			{
				diff1.bits[0] = (cur->ipr.ip4r.upper - pageunion.ipr.ip4r.lower);
				diff2.bits[0] = (pageunion.ipr.ip4r.upper - cur->ipr.ip4r.lower);
				diff1.bits[1] = diff2.bits[1] = 0;
			}
			else
			{
				ip6_sub(&cur->ipr.ip6r.upper, &pageunion.ipr.ip6r.lower, &diff1);
				ip6_sub(&pageunion.ipr.ip6r.upper, &cur->ipr.ip6r.lower, &diff2);
			}
			switch (ip6_compare(&diff1,&diff2))
			{
				case -1:
					ADDLIST(listL, unionL, posL, arr[i].pos);
					break;
				case 0:
					if (posL > posR)
						ADDLIST(listR, unionR, posR, arr[i].pos);
					else
						ADDLIST(listL, unionL, posL, arr[i].pos);
					break;
				case 1:
					ADDLIST(listR, unionR, posR, arr[i].pos);
					break;
			}
		}

        pfree(arr);
    }

    v->spl_nleft = posL;
    v->spl_nright = posR;

    PG_RETURN_POINTER(v);
}

#undef ADDLIST

/*
** Equality methods
*/
PG_FUNCTION_INFO_V1(gipr_same);
Datum
gipr_same(PG_FUNCTION_ARGS)
{
    IPR_KEY *v1 = (IPR_KEY *) PG_GETARG_POINTER(0);
    IPR_KEY *v2 = (IPR_KEY *) PG_GETARG_POINTER(1);
    bool *result = (bool *) PG_GETARG_POINTER(2);

	if (!v1 || !v2)
		*result = (v1 == NULL && v2 == NULL);
	if (v1->af != v2->af)
		*result = false;
	else
	{
		switch (v1->af)
		{
			case 0:
				*result = true;
				break;

			case PGSQL_AF_INET:
				*result = ip4r_equal(&v1->ipr.ip4r,&v2->ipr.ip4r);
				break;

			case PGSQL_AF_INET6:
				*result = ip6r_equal(&v1->ipr.ip6r,&v2->ipr.ip6r);
				break;
		}
	}

#ifdef GIST_DEBUG
    fprintf(stderr, "same: %s\n", (*result ? "true" : "false"));
#endif

    PG_RETURN_POINTER(result);
}


/*
 * Strategy numbers:
 *      OPERATOR        1       >>= ,
 *      OPERATOR        2       <<= ,
 *      OPERATOR        3       >> ,
 *      OPERATOR        4       << ,
 *      OPERATOR        5       && ,
 *      OPERATOR        6       = ,
 */

/*
** SUPPORT ROUTINES
*/

static bool
gipr_leaf_consistent(IPR_KEY *key,
					 IPR_P queryp,
					 StrategyNumber strategy)
{
	IPR query;
	int af = ipr_unpack(queryp, &query);

#ifdef GIST_QUERY_DEBUG
    fprintf(stderr, "leaf_consistent, %d\n", strategy);
#endif

	if (key->af == 0)
	{
		switch (strategy)
		{
			case 1:   /* left contains right nonstrict */
				return true;
			case 2:   /* left contained in right nonstrict */
				return (af == 0);
			case 3:   /* left contains right strict */
				return !(af == 0);
			case 4:   /* left contained in right strict */
				return false;
			case 5:   /* left overlaps right */
				return true;
			case 6:   /* left equal right */
				return (af == 0);
		}
    }
	else if (af == 0)
	{
		switch (strategy)
		{
			case 1:   /* left contains right nonstrict */
				return false;
			case 2:   /* left contained in right nonstrict */
				return true;
			case 3:   /* left contains right strict */
				return false;
			case 4:   /* left contained in right strict */
				return true;
			case 5:   /* left overlaps right */
				return true;
			case 6:   /* left equal right */
				return false;
		}
	}
	else if (af != key->af)
		return false;
	else if (af == PGSQL_AF_INET)
	{
		switch (strategy)
		{
			case 1:   /* left contains right nonstrict */
				return ip4r_contains_internal(&key->ipr.ip4r, &query.ip4r, true);
			case 2:   /* left contained in right nonstrict */
				return ip4r_contains_internal(&query.ip4r, &key->ipr.ip4r, true);
			case 3:   /* left contains right strict */
				return ip4r_contains_internal(&key->ipr.ip4r, &query.ip4r, false);
			case 4:   /* left contained in right strict */
				return ip4r_contains_internal(&query.ip4r, &key->ipr.ip4r, false);
			case 5:   /* left overlaps right */
				return ip4r_overlaps_internal(&key->ipr.ip4r, &query.ip4r);
			case 6:   /* left equal right */
				return ip4r_equal(&key->ipr.ip4r, &query.ip4r);
		}
	}
	else if (af == PGSQL_AF_INET6)
	{
		switch (strategy)
		{
			case 1:   /* left contains right nonstrict */
				return ip6r_contains_internal(&key->ipr.ip6r, &query.ip6r, true);
			case 2:   /* left contained in right nonstrict */
				return ip6r_contains_internal(&query.ip6r, &key->ipr.ip6r, true);
			case 3:   /* left contains right strict */
				return ip6r_contains_internal(&key->ipr.ip6r, &query.ip6r, false);
			case 4:   /* left contained in right strict */
				return ip6r_contains_internal(&query.ip6r, &key->ipr.ip6r, false);
			case 5:   /* left overlaps right */
				return ip6r_overlaps_internal(&key->ipr.ip6r, &query.ip6r);
			case 6:   /* left equal right */
				return ip6r_equal(&key->ipr.ip6r, &query.ip6r);
		}
    }
	return false;
}

/* logic notes:
 * If the union value we're looking at overlaps with our query value
 * at all, then any of the values underneath it might overlap with us
 * or be contained by us, so all the "contained by" and "overlaps"
 * cases degenerate to "overlap".
 * If the union value is equal to the query value, then none of the
 * values under it can strictly contain the query value, so for
 * "contained" queries the strictness is preserved.
 * If we're looking for an "equal" value, then we have to enter any
 * subtree whose union contains (not strictly) our query value.
 */

static bool
gipr_internal_consistent(IPR_KEY *key,
						 IPR_P queryp,
						 StrategyNumber strategy)
{
	IPR query;
	int af = ipr_unpack(queryp, &query);

#ifdef GIST_QUERY_DEBUG
    fprintf(stderr, "leaf_consistent, %d\n", strategy);
#endif

	if (af == 0 && strategy == 4)
		return false;
	else if (key->af == 0 || af == 0)
		return true;
	else if (af != key->af)
		return false;
	else if (af == PGSQL_AF_INET)
	{
		switch (strategy)
		{
			case 2:   /* left contained in right nonstrict */
			case 4:   /* left contained in right strict */
			case 5:   /* left overlaps right */
				return ip4r_overlaps_internal(&key->ipr.ip4r, &query.ip4r);
			case 3:   /* left contains right strict */
				return ip4r_contains_internal(&key->ipr.ip4r, &query.ip4r, false);
			case 1:   /* left contains right nonstrict */
			case 6:   /* left equal right */
				return ip4r_contains_internal(&key->ipr.ip4r, &query.ip4r, true);
		}
	}
	else if (af == PGSQL_AF_INET6)
	{
		switch (strategy)
		{
			case 2:   /* left contained in right nonstrict */
			case 4:   /* left contained in right strict */
			case 5:   /* left overlaps right */
				return ip6r_overlaps_internal(&key->ipr.ip6r, &query.ip6r);
			case 3:   /* left contains right strict */
				return ip6r_contains_internal(&key->ipr.ip6r, &query.ip6r, false);
			case 1:   /* left contains right nonstrict */
			case 6:   /* left equal right */
				return ip6r_contains_internal(&key->ipr.ip6r, &query.ip6r, true);
		}
    }
	return false;
}

/* end */
