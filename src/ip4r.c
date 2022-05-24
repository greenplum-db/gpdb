/* ip4r.c */

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

/*
 * extract an IP range from text.
 */
static
bool ip4r_from_str(char *str, IP4R *ipr)
{
    char buf[IP4_STRING_MAX];
    int pos = strcspn(str, "-/");
    IP4 ip;

    switch (str[pos])
    {
        case 0:     /* no separator, must be single ip4 addr */
        {
            if (!ip4_raw_input(str, &ip))
                return false;
            ipr->lower = ip;
            ipr->upper = ip;
            return true;
        }

        case '-':   /* lower-upper */
        {
            char *rest = str + pos + 1;

            if (pos >= sizeof(buf))
                return false;
            memcpy(buf, str, pos);
            buf[pos] = 0;
            if (!ip4_raw_input(buf, &ip))
                return false;
            ipr->lower = ip;
            if (!ip4_raw_input(rest, &ip))
                return false;
            if (!ip4_lessthan(ip, ipr->lower))
                ipr->upper = ip;
            else
            {
                ipr->upper = ipr->lower;
                ipr->lower = ip;
            }
            return true;
        }

        case '/':  /* prefix/len */
        {
            char *rest = str + pos + 1;
            unsigned pfxlen;
            char dummy;

            if (pos >= sizeof(buf))
                return false;
            memcpy(buf, str, pos);
            buf[pos] = 0;
            if (!ip4_raw_input(buf, &ip))
                return false;
            if (rest[strspn(rest,"0123456789")])
                return false;
            if (sscanf(rest, "%u%c", &pfxlen, &dummy) != 1)
                return false;
            return ip4r_from_cidr(ip, pfxlen, ipr);
        }

        default:
            return false;      /* can't happen */
    }
}


/* Output an ip range in text form
 */
static inline
int ip4r_to_str(IP4R *ipr, char *str, int slen)
{
    char buf1[IP4_STRING_MAX];
    char buf2[IP4_STRING_MAX];
    unsigned msk;

    if (ip4_equal(ipr->lower, ipr->upper))
        return ip4_raw_output(ipr->lower, str, slen);

    if ((msk = masklen(ipr->lower,ipr->upper)) <= 32)
    {
        ip4_raw_output(ipr->lower, buf1, sizeof(buf1));
        return snprintf(str, slen, "%s/%u", buf1, msk);
    }

    ip4_raw_output(ipr->lower, buf1, sizeof(buf1));
    ip4_raw_output(ipr->upper, buf2, sizeof(buf2));

    return snprintf(str, slen, "%s-%s", buf1, buf2);
}


/**************************************************************************/
/* This part handles all aspects of postgres interfacing.
 */

static
text *
make_text(int len)
{
    text *ret = (text *) palloc0(len + VARHDRSZ);
    SET_VARSIZE(ret, len + VARHDRSZ);
    return ret;
}

static inline
void
set_text_len(text *txt, int len)
{
	Assert(len + VARHDRSZ <= VARSIZE(txt));
	if (len + VARHDRSZ <= VARSIZE(txt))
		SET_VARSIZE(txt, len + VARHDRSZ);
}

/*
** Input/Output routines
*/

PG_FUNCTION_INFO_V1(ip4_in);
Datum
ip4_in(PG_FUNCTION_ARGS)
{
    char *str = PG_GETARG_CSTRING(0);
    IP4 ip;
    if (ip4_raw_input(str, &ip))
        PG_RETURN_IP4(ip);

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP4 value: '%s'", str)));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4_out);
Datum
ip4_out(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    char *out = palloc(IP4_STRING_MAX);
    ip4_raw_output(ip, out, IP4_STRING_MAX);
    PG_RETURN_CSTRING(out);
}

PG_FUNCTION_INFO_V1(ip4_recv);
Datum
ip4_recv(PG_FUNCTION_ARGS)
{
    StringInfo buf = (StringInfo) PG_GETARG_POINTER(0);
    PG_RETURN_IP4((IP4) pq_getmsgint(buf, sizeof(IP4)));
}

PG_FUNCTION_INFO_V1(ip4_send);
Datum
ip4_send(PG_FUNCTION_ARGS)
{
    IP4 arg1 = PG_GETARG_IP4(0);
    StringInfoData buf;

    pq_begintypsend(&buf);
    pq_sendint(&buf, arg1, sizeof(IP4));
    PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

PG_FUNCTION_INFO_V1(ip4hash);
Datum
ip4hash(PG_FUNCTION_ARGS)
{
    IP4 arg1 = PG_GETARG_IP4(0);

    return hash_any((unsigned char *)&arg1, sizeof(IP4));
}

PG_FUNCTION_INFO_V1(ip4_hash_extended);
Datum
ip4_hash_extended(PG_FUNCTION_ARGS)
{
    IP4 arg1 = PG_GETARG_IP4(0);
	uint64 seed = DatumGetUInt64(PG_GETARG_DATUM(1));

    return hash_any_extended((unsigned char *)&arg1, sizeof(IP4), seed);
}

PG_FUNCTION_INFO_V1(ip4_cast_to_text);
Datum
ip4_cast_to_text(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    text *out = make_text(IP4_STRING_MAX);
    set_text_len(out, ip4_raw_output(ip, VARDATA(out), IP4_STRING_MAX));
    PG_RETURN_TEXT_P(out);
}

PG_FUNCTION_INFO_V1(ip4_cast_from_text);
Datum
ip4_cast_from_text(PG_FUNCTION_ARGS)
{
    text *txt = PG_GETARG_TEXT_PP(0);
    int tlen = VARSIZE_ANY_EXHDR(txt);
    char buf[IP4_STRING_MAX];

    if (tlen < sizeof(buf))
    {
        IP4 ip;

        memcpy(buf, VARDATA_ANY(txt), tlen);
        buf[tlen] = 0;
        if (ip4_raw_input(buf, &ip))
            PG_RETURN_IP4(ip);
    }

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP4 value in text")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4_cast_from_inet);
Datum
ip4_cast_from_inet(PG_FUNCTION_ARGS)
{
    inet *inetptr = PG_GETARG_INET_P(0);
    inet_struct *in = INET_STRUCT_DATA(inetptr);

    if (in->family == PGSQL_AF_INET)
    {
        unsigned char *p = in->ipaddr;
        IP4 ip = (p[0] << 24)|(p[1] << 16)|(p[2] << 8)|p[3];
        PG_RETURN_IP4(ip);
    }

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid INET value for conversion to IP4")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4_cast_to_cidr);
Datum
ip4_cast_to_cidr(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    inet *res = palloc0(VARHDRSZ + sizeof(inet_struct));
    inet_struct *in;

    SET_VARSIZE(res, VARHDRSZ + offsetof(inet_struct, ipaddr) + 4);

    in = ((inet_struct *)VARDATA(res));
    in->bits = 32;
    in->family = PGSQL_AF_INET;
    {
        unsigned char *p = in->ipaddr;
        p[0] = (ip >> 24) & 0xff;
        p[1] = (ip >> 16) & 0xff;
        p[2] = (ip >>  8) & 0xff;
        p[3] = (ip      ) & 0xff;
    }

    PG_RETURN_INET_P(res);
}

PG_FUNCTION_INFO_V1(ip4_cast_to_bigint);
Datum
ip4_cast_to_bigint(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    PG_RETURN_INT64(ip);
}

PG_FUNCTION_INFO_V1(ip4_cast_to_numeric);
Datum
ip4_cast_to_numeric(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
	int64 v = ip;
	PG_RETURN_DATUM(DirectFunctionCall1(int8_numeric, Int64GetDatumFast(v)));
}

PG_FUNCTION_INFO_V1(ip4_cast_from_bigint);
Datum
ip4_cast_from_bigint(PG_FUNCTION_ARGS)
{
    int64 val = PG_GETARG_INT64(0);

	if (val < -(int64)0x80000000UL || val > (int64)0xFFFFFFFFUL)
        ereport(ERROR,
                (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                 errmsg("ip address out of range")));

    PG_RETURN_IP4(val);
}

PG_FUNCTION_INFO_V1(ip4_cast_to_double);
Datum
ip4_cast_to_double(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    PG_RETURN_FLOAT8(ip);
}

PG_FUNCTION_INFO_V1(ip4_cast_from_double);
Datum
ip4_cast_from_double(PG_FUNCTION_ARGS)
{
    float8 val = PG_GETARG_FLOAT8(0);
    float8 ival = 0;

    if (modf(val,&ival) != 0.0)
    {
        ereport(WARNING,
                (errcode(ERRCODE_WARNING),
                 errmsg("double converted to IP4 is not integral")));
    }

	if (ival < -(float8)0x80000000UL || ival > (float8)0xFFFFFFFFUL)
        ereport(ERROR,
                (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                 errmsg("ip address out of range")));

	/*
	 * casting directly to ulong evokes the nasal demons for negative values,
	 * casting to long first evokes them for large positive values if long is
	 * 32bit.
	 */
	if (ival < 0)
		PG_RETURN_IP4((unsigned long) (long) ival);
	else
		PG_RETURN_IP4((unsigned long) ival);
}

PG_FUNCTION_INFO_V1(ip4_cast_from_numeric);
Datum
ip4_cast_from_numeric(PG_FUNCTION_ARGS)
{
    Datum val_num = PG_GETARG_DATUM(0);
	int64 val = DatumGetInt64(DirectFunctionCall1(numeric_int8,val_num));

	if (val < -(int64)0x80000000UL || val > (int64)0xFFFFFFFFUL)
        ereport(ERROR,
                (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                 errmsg("ip address out of range")));

    PG_RETURN_IP4((unsigned long) val);
}

PG_FUNCTION_INFO_V1(ip4_cast_from_bit);
Datum
ip4_cast_from_bit(PG_FUNCTION_ARGS)
{
	VarBit *val = PG_GETARG_VARBIT_P(0);

	if (val->bit_len == 32)
	{
        bits8 *p = VARBITS(val);
        IP4 ip = (p[0] << 24)|(p[1] << 16)|(p[2] << 8)|p[3];
        PG_RETURN_IP4(ip);
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid BIT value for conversion to IP4")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4_cast_to_bit);
Datum
ip4_cast_to_bit(PG_FUNCTION_ARGS)
{
	IP4 ip = PG_GETARG_IP4(0);
	int len = VARBITTOTALLEN(32);
	VarBit *res = palloc0(len);
	unsigned char *p = VARBITS(res);

	SET_VARSIZE(res, len);
	VARBITLEN(res) = 32;

	p[0] = (ip >> 24) & 0xff;
	p[1] = (ip >> 16) & 0xff;
	p[2] = (ip >>  8) & 0xff;
	p[3] = (ip      ) & 0xff;

	PG_RETURN_VARBIT_P(res);
}

PG_FUNCTION_INFO_V1(ip4_cast_from_bytea);
Datum
ip4_cast_from_bytea(PG_FUNCTION_ARGS)
{
	void *val = PG_GETARG_BYTEA_PP(0);

	if (VARSIZE_ANY_EXHDR(val) == 4)
	{
        unsigned char *p = (unsigned char *) VARDATA_ANY(val);
        IP4 ip = (p[0] << 24)|(p[1] << 16)|(p[2] << 8)|p[3];
        PG_RETURN_IP4(ip);
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid BYTEA value for conversion to IP4")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4_cast_to_bytea);
Datum
ip4_cast_to_bytea(PG_FUNCTION_ARGS)
{
	IP4 ip = PG_GETARG_IP4(0);
	bytea *res = palloc(VARHDRSZ + 4);
	unsigned char *p = (unsigned char *) VARDATA(res);

	SET_VARSIZE(res, VARHDRSZ + 4);

	p[0] = (ip >> 24) & 0xff;
	p[1] = (ip >> 16) & 0xff;
	p[2] = (ip >>  8) & 0xff;
	p[3] = (ip      ) & 0xff;

	PG_RETURN_BYTEA_P(res);
}

PG_FUNCTION_INFO_V1(ip4_netmask);
Datum
ip4_netmask(PG_FUNCTION_ARGS)
{
    int pfxlen = PG_GETARG_INT32(0);

    if (pfxlen < 0 || pfxlen > 32)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("prefix length out of range")));
    }

    PG_RETURN_IP4( netmask(pfxlen) );
}

PG_FUNCTION_INFO_V1(ip4_net_lower);
Datum
ip4_net_lower(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    int pfxlen = PG_GETARG_INT32(1);

    if (pfxlen < 0 || pfxlen > 32)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("prefix length out of range")));
    }

    PG_RETURN_IP4( ip & netmask(pfxlen) );
}

PG_FUNCTION_INFO_V1(ip4_net_upper);
Datum
ip4_net_upper(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    int pfxlen = PG_GETARG_INT32(1);

    if (pfxlen < 0 || pfxlen > 32)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("prefix length out of range")));
    }

    PG_RETURN_IP4( ip | hostmask(pfxlen) );
}

PG_FUNCTION_INFO_V1(ip4_plus_int);
Datum
ip4_plus_int(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    int addend = PG_GETARG_INT32(1);
    IP4 result = ip + (IP4) addend;

    if ((addend < 0) != (result < ip))
    {
        ereport(ERROR,
                (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                 errmsg("ip address out of range")));
    }

    PG_RETURN_IP4(result);
}

PG_FUNCTION_INFO_V1(ip4_plus_bigint);
Datum
ip4_plus_bigint(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    int64 addend = PG_GETARG_INT64(1);
    int64 result = (int64) ip + addend;

    if (((addend < 0) != (result < ip))
        || result != (int64)(IP4)result)
    {
        ereport(ERROR,
                (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                 errmsg("ip address out of range")));
    }

    PG_RETURN_IP4( (IP4)(result) );
}

PG_FUNCTION_INFO_V1(ip4_plus_numeric);
Datum
ip4_plus_numeric(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    Datum addend_num = PG_GETARG_DATUM(1);
	int64 addend = DatumGetInt64(DirectFunctionCall1(numeric_int8,addend_num));
    int64 result = (int64) ip + addend;

    if (((addend < 0) != (result < ip))
        || result != (int64)(IP4)result)
    {
        ereport(ERROR,
                (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                 errmsg("ip address out of range")));
    }

    PG_RETURN_IP4( (IP4)(result) );
}

PG_FUNCTION_INFO_V1(ip4_minus_int);
Datum
ip4_minus_int(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    int subtrahend = PG_GETARG_INT32(1);
    IP4 result = ip - (IP4) subtrahend;

    if ((subtrahend > 0) != (result < ip))
    {
        ereport(ERROR,
                (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                 errmsg("ip address out of range")));
    }

    PG_RETURN_IP4(result);
}

PG_FUNCTION_INFO_V1(ip4_minus_bigint);
Datum
ip4_minus_bigint(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    int64 subtrahend = PG_GETARG_INT64(1);
    int64 result = (int64) ip - subtrahend;

    if (((subtrahend > 0) != (result < ip))
        || result != (int64)(IP4)result)
    {
        ereport(ERROR,
                (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                 errmsg("ip address out of range")));
    }

    PG_RETURN_IP4( (IP4)(result) );
}

PG_FUNCTION_INFO_V1(ip4_minus_numeric);
Datum
ip4_minus_numeric(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
	Datum subtrahend_num = PG_GETARG_DATUM(1);
	int64 subtrahend = DatumGetInt64(DirectFunctionCall1(numeric_int8,subtrahend_num));
    int64 result = (int64) ip - subtrahend;

    if (((subtrahend > 0) != (result < ip))
        || result != (int64)(IP4)result)
    {
        ereport(ERROR,
                (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                 errmsg("ip address out of range")));
    }

    PG_RETURN_IP4( (IP4)(result) );
}

PG_FUNCTION_INFO_V1(ip4_minus_ip4);
Datum
ip4_minus_ip4(PG_FUNCTION_ARGS)
{
    IP4 minuend = PG_GETARG_IP4(0);
    IP4 subtrahend = PG_GETARG_IP4(1);
    int64 result = (int64) minuend - (int64) subtrahend;

    PG_RETURN_INT64(result);
}

PG_FUNCTION_INFO_V1(ip4_and);
Datum
ip4_and(PG_FUNCTION_ARGS)
{
    IP4 a = PG_GETARG_IP4(0);
    IP4 b = PG_GETARG_IP4(1);

    PG_RETURN_IP4(a & b);
}

PG_FUNCTION_INFO_V1(ip4_or);
Datum
ip4_or(PG_FUNCTION_ARGS)
{
    IP4 a = PG_GETARG_IP4(0);
    IP4 b = PG_GETARG_IP4(1);

    PG_RETURN_IP4(a | b);
}

PG_FUNCTION_INFO_V1(ip4_xor);
Datum
ip4_xor(PG_FUNCTION_ARGS)
{
    IP4 a = PG_GETARG_IP4(0);
    IP4 b = PG_GETARG_IP4(1);

    PG_RETURN_IP4(a ^ b);
}

PG_FUNCTION_INFO_V1(ip4_not);
Datum
ip4_not(PG_FUNCTION_ARGS)
{
    IP4 a = PG_GETARG_IP4(0);

    PG_RETURN_IP4(~a);
}


/*---- ip4r ----*/

PG_FUNCTION_INFO_V1(ip4r_in);
Datum
ip4r_in(PG_FUNCTION_ARGS)
{
    char *str = PG_GETARG_CSTRING(0);
    IP4R ipr;
    if (ip4r_from_str(str, &ipr))
    {
        IP4R *res = palloc(sizeof(IP4R));
        *res = ipr;
        PG_RETURN_IP4R_P(res);
    }

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP4R value: \"%s\"", str)));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4r_out);
Datum
ip4r_out(PG_FUNCTION_ARGS)
{
    IP4R *ipr = PG_GETARG_IP4R_P(0);
    char *out = palloc(IP4R_STRING_MAX);
    ip4r_to_str(ipr, out, IP4R_STRING_MAX);
    PG_RETURN_CSTRING(out);
}

PG_FUNCTION_INFO_V1(ip4r_recv);
Datum
ip4r_recv(PG_FUNCTION_ARGS)
{
    StringInfo buf = (StringInfo) PG_GETARG_POINTER(0);
    IP4R *ipr = palloc(sizeof(IP4R));

    ipr->lower = (IP4) pq_getmsgint(buf, sizeof(IP4));
    ipr->upper = (IP4) pq_getmsgint(buf, sizeof(IP4));

    PG_RETURN_IP4R_P(ipr);
}

PG_FUNCTION_INFO_V1(ip4r_send);
Datum
ip4r_send(PG_FUNCTION_ARGS)
{
    IP4R *ipr = PG_GETARG_IP4R_P(0);
    StringInfoData buf;

    pq_begintypsend(&buf);
    pq_sendint(&buf, ipr->lower, sizeof(IP4));
    pq_sendint(&buf, ipr->upper, sizeof(IP4));
    PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

PG_FUNCTION_INFO_V1(ip4rhash);
Datum
ip4rhash(PG_FUNCTION_ARGS)
{
    IP4R *arg1 = PG_GETARG_IP4R_P(0);

    return hash_any((unsigned char *)arg1, sizeof(IP4R));
}

PG_FUNCTION_INFO_V1(ip4r_hash_extended);
Datum
ip4r_hash_extended(PG_FUNCTION_ARGS)
{
    IP4R *arg1 = PG_GETARG_IP4R_P(0);
	uint64 seed = DatumGetUInt64(PG_GETARG_DATUM(1));

    return hash_any_extended((unsigned char *)arg1, sizeof(IP4R), seed);
}

PG_FUNCTION_INFO_V1(ip4r_cast_to_text);
Datum
ip4r_cast_to_text(PG_FUNCTION_ARGS)
{
    IP4R *ipr = PG_GETARG_IP4R_P(0);
    text *out = make_text(IP4R_STRING_MAX);
    set_text_len(out, ip4r_to_str(ipr, VARDATA(out), IP4R_STRING_MAX));
    PG_RETURN_TEXT_P(out);
}

PG_FUNCTION_INFO_V1(ip4r_cast_from_text);
Datum
ip4r_cast_from_text(PG_FUNCTION_ARGS)
{
    text *txt = PG_GETARG_TEXT_PP(0);
    int tlen = VARSIZE_ANY_EXHDR(txt);
    char buf[IP4R_STRING_MAX];

    if (tlen < sizeof(buf))
    {
        IP4R ipr;

        memcpy(buf, VARDATA_ANY(txt), tlen);
        buf[tlen] = 0;
        if (ip4r_from_str(buf, &ipr))
        {
            IP4R *res = palloc(sizeof(IP4R));
            *res = ipr;
            PG_RETURN_IP4R_P(res);
        }
    }

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP4R value in text")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4r_cast_from_cidr);
Datum
ip4r_cast_from_cidr(PG_FUNCTION_ARGS)
{
    inet *inetptr = PG_GETARG_INET_P(0);
    inet_struct *in = INET_STRUCT_DATA(inetptr);

    if (in->family == PGSQL_AF_INET)
    {
        unsigned char *p = in->ipaddr;
        IP4 ip = (p[0] << 24)|(p[1] << 16)|(p[2] << 8)|p[3];
        IP4R ipr;
        if (ip4r_from_cidr(ip, in->bits, &ipr))
        {
            IP4R *res = palloc(sizeof(IP4R));
            *res = ipr;
            PG_RETURN_IP4R_P(res);
        }
    }

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid CIDR value for conversion to IP4R")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4r_cast_to_cidr);
Datum
ip4r_cast_to_cidr(PG_FUNCTION_ARGS)
{
    IP4R *ipr = PG_GETARG_IP4R_P(0);
    IP4 ip = ipr->lower;
    inet *res;
    inet_struct *in;
    unsigned bits = masklen(ip, ipr->upper);

    if (bits > 32)
        PG_RETURN_NULL();

    res = palloc0(VARHDRSZ + sizeof(inet_struct));
    SET_VARSIZE(res, VARHDRSZ + offsetof(inet_struct, ipaddr) + 4);

    in = ((inet_struct *)VARDATA(res));
    in->bits = bits;
    in->family = PGSQL_AF_INET;
    {
        unsigned char *p = in->ipaddr;
        p[0] = (ip >> 24) & 0xff;
        p[1] = (ip >> 16) & 0xff;
        p[2] = (ip >>  8) & 0xff;
        p[3] = (ip      ) & 0xff;
    }

    PG_RETURN_INET_P(res);
}

PG_FUNCTION_INFO_V1(ip4r_cast_from_ip4);
Datum
ip4r_cast_from_ip4(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    IP4R *res = palloc(sizeof(IP4R));
    if (ip4r_from_inet(ip, 32, res))
    {
        PG_RETURN_IP4R_P(res);
    }

    pfree(res);
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid IP4 value for conversion to IP4R (shouldn't be possible)")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4r_from_ip4s);
Datum
ip4r_from_ip4s(PG_FUNCTION_ARGS)
{
    IP4 a = PG_GETARG_IP4(0);
    IP4 b = PG_GETARG_IP4(1);
    IP4R *res = palloc(sizeof(IP4R));
    if (a < b)
        res->lower = a, res->upper = b;
    else
        res->lower = b, res->upper = a;
    PG_RETURN_IP4R_P( res );
}

PG_FUNCTION_INFO_V1(ip4r_cast_from_bit);
Datum
ip4r_cast_from_bit(PG_FUNCTION_ARGS)
{
	VarBit *val = PG_GETARG_VARBIT_P(0);
	int bitlen = VARBITLEN(val);

	if (bitlen <= 32)
	{
		bits8 buf[4];
        bits8 *p = VARBITS(val);
		IP4 ip;
		IP4R *res = palloc(sizeof(IP4R));

		if (bitlen <= 24)
		{
			memset(buf, 0, sizeof(buf));
			memcpy(buf, p, VARBITBYTES(val));
			p = buf;
		}

        ip = (p[0] << 24)|(p[1] << 16)|(p[2] << 8)|p[3];

		if (ip4r_from_cidr(ip, bitlen, res))
			PG_RETURN_IP4R_P(res);
	}

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("invalid BIT value for conversion to IP4R")));
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4r_cast_to_bit);
Datum
ip4r_cast_to_bit(PG_FUNCTION_ARGS)
{
    IP4R *ipr = PG_GETARG_IP4R_P(0);
    IP4 ip = ipr->lower;
    unsigned bits = masklen(ip, ipr->upper);
	VarBit *res;
	unsigned char buf[4];
	int len;

    if (bits > 32)
        PG_RETURN_NULL();

	len = VARBITTOTALLEN(bits);
    res = palloc0(len);
    SET_VARSIZE(res, len);
	VARBITLEN(res) = bits;

	buf[0] = (ip >> 24) & 0xff;
	buf[1] = (ip >> 16) & 0xff;
	buf[2] = (ip >>  8) & 0xff;
	buf[3] = (ip      ) & 0xff;

	memcpy(VARBITS(res), buf, VARBITBYTES(res));
	PG_RETURN_VARBIT_P(res);
}

PG_FUNCTION_INFO_V1(ip4r_net_prefix);
Datum
ip4r_net_prefix(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    int pfxlen = PG_GETARG_INT32(1);

    if (pfxlen < 0 || pfxlen > 32)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("prefix length out of range")));
    }

    {
        IP4 mask = netmask(pfxlen);
        IP4R *res = palloc(sizeof(IP4R));

        res->lower = ip & mask;
        res->upper = ip | ~mask;

        PG_RETURN_IP4R_P(res);
    }
}

PG_FUNCTION_INFO_V1(ip4r_net_mask);
Datum
ip4r_net_mask(PG_FUNCTION_ARGS)
{
    IP4 ip = PG_GETARG_IP4(0);
    IP4 mask = PG_GETARG_IP4(1);

    if (!ip4_valid_netmask(mask))
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("invalid netmask")));
    }

    {
        IP4R *res = palloc(sizeof(IP4R));

        res->lower = ip & mask;
        res->upper = ip | ~mask;

        PG_RETURN_IP4R_P(res);
    }
}

PG_FUNCTION_INFO_V1(ip4r_lower);
Datum
ip4r_lower(PG_FUNCTION_ARGS)
{
    IP4R *ipr = PG_GETARG_IP4R_P(0);
    PG_RETURN_IP4( ipr->lower );
}

PG_FUNCTION_INFO_V1(ip4r_upper);
Datum
ip4r_upper(PG_FUNCTION_ARGS)
{
    IP4R *ipr = PG_GETARG_IP4R_P(0);
    PG_RETURN_IP4( ipr->upper );
}

PG_FUNCTION_INFO_V1(ip4r_is_cidr);
Datum
ip4r_is_cidr(PG_FUNCTION_ARGS)
{
    IP4R *ipr = PG_GETARG_IP4R_P(0);
    PG_RETURN_BOOL( (masklen(ipr->lower,ipr->upper) <= 32U) );
}

/*
 * Decompose an arbitrary range into CIDRs
 */

PG_FUNCTION_INFO_V1(ip4r_cidr_split);
Datum
ip4r_cidr_split(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	IP4R *val;
	IP4R *res;

	if (SRF_IS_FIRSTCALL())
	{
		IP4R *in = PG_GETARG_IP4R_P(0);
		funcctx = SRF_FIRSTCALL_INIT();
		val = MemoryContextAlloc(funcctx->multi_call_memory_ctx,
								 sizeof(IP4R));
		*val = *in;
		funcctx->user_fctx = val;
	}

	funcctx = SRF_PERCALL_SETUP();
	val = funcctx->user_fctx;
	if (!val)
		SRF_RETURN_DONE(funcctx);

	res = palloc(sizeof(IP4R));
	if (ip4r_split_cidr(val, res))
		funcctx->user_fctx = NULL;

	SRF_RETURN_NEXT(funcctx, IP4RPGetDatum(res));
}

/*
 * comparisons and indexing
 */

PG_FUNCTION_INFO_V1(ip4_lt);
Datum
ip4_lt(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4_lessthan(PG_GETARG_IP4(0), PG_GETARG_IP4(1)) );
}

PG_FUNCTION_INFO_V1(ip4_le);
Datum
ip4_le(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4_less_eq(PG_GETARG_IP4(0), PG_GETARG_IP4(1)) );
}

PG_FUNCTION_INFO_V1(ip4_gt);
Datum
ip4_gt(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4_lessthan(PG_GETARG_IP4(1), PG_GETARG_IP4(0)) );
}

PG_FUNCTION_INFO_V1(ip4_ge);
Datum
ip4_ge(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4_less_eq(PG_GETARG_IP4(1), PG_GETARG_IP4(0)) );
}

PG_FUNCTION_INFO_V1(ip4_eq);
Datum
ip4_eq(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4_equal(PG_GETARG_IP4(0), PG_GETARG_IP4(1)) );
}

PG_FUNCTION_INFO_V1(ip4_neq);
Datum
ip4_neq(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( !ip4_equal(PG_GETARG_IP4(0), PG_GETARG_IP4(1)) );
}

PG_FUNCTION_INFO_V1(ip4r_lt);
Datum
ip4r_lt(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_lessthan(PG_GETARG_IP4R_P(0), PG_GETARG_IP4R_P(1)) );
}

PG_FUNCTION_INFO_V1(ip4r_le);
Datum
ip4r_le(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_less_eq(PG_GETARG_IP4R_P(0), PG_GETARG_IP4R_P(1)) );
}

PG_FUNCTION_INFO_V1(ip4r_gt);
Datum
ip4r_gt(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_lessthan(PG_GETARG_IP4R_P(1), PG_GETARG_IP4R_P(0)) );
}

PG_FUNCTION_INFO_V1(ip4r_ge);
Datum
ip4r_ge(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_less_eq(PG_GETARG_IP4R_P(1), PG_GETARG_IP4R_P(0)) );
}

PG_FUNCTION_INFO_V1(ip4r_eq);
Datum
ip4r_eq(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_equal(PG_GETARG_IP4R_P(0), PG_GETARG_IP4R_P(1)) );
}

PG_FUNCTION_INFO_V1(ip4r_neq);
Datum
ip4r_neq(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( !ip4r_equal(PG_GETARG_IP4R_P(0), PG_GETARG_IP4R_P(1)) );
}

PG_FUNCTION_INFO_V1(ip4r_overlaps);
Datum
ip4r_overlaps(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_overlaps_internal(PG_GETARG_IP4R_P(0),
                                           PG_GETARG_IP4R_P(1)) );
}

PG_FUNCTION_INFO_V1(ip4r_contains);
Datum
ip4r_contains(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_contains_internal(PG_GETARG_IP4R_P(0),
                                           PG_GETARG_IP4R_P(1),
                                           true) );
}

PG_FUNCTION_INFO_V1(ip4r_contains_strict);
Datum
ip4r_contains_strict(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_contains_internal(PG_GETARG_IP4R_P(0),
                                           PG_GETARG_IP4R_P(1),
                                           false) );
}

PG_FUNCTION_INFO_V1(ip4r_contained_by);
Datum
ip4r_contained_by(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_contains_internal(PG_GETARG_IP4R_P(1),
                                           PG_GETARG_IP4R_P(0),
                                           true) );
}

PG_FUNCTION_INFO_V1(ip4r_contained_by_strict);
Datum
ip4r_contained_by_strict(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_contains_internal(PG_GETARG_IP4R_P(1),
                                           PG_GETARG_IP4R_P(0),
                                           false) );
}

PG_FUNCTION_INFO_V1(ip4_contains);
Datum
ip4_contains(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4_contains_internal(PG_GETARG_IP4R_P(0), PG_GETARG_IP4(1)) );
}

PG_FUNCTION_INFO_V1(ip4_contained_by);
Datum
ip4_contained_by(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4_contains_internal(PG_GETARG_IP4R_P(1), PG_GETARG_IP4(0)) );
}

PG_FUNCTION_INFO_V1(ip4r_left_of);
Datum
ip4r_left_of(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_left_internal(PG_GETARG_IP4R_P(0), PG_GETARG_IP4R_P(1)) );
}

PG_FUNCTION_INFO_V1(ip4r_right_of);
Datum
ip4r_right_of(PG_FUNCTION_ARGS)
{
    PG_RETURN_BOOL( ip4r_left_internal(PG_GETARG_IP4R_P(1), PG_GETARG_IP4R_P(0)) );
}


PG_FUNCTION_INFO_V1(ip4r_union);
Datum
ip4r_union(PG_FUNCTION_ARGS)
{
    IP4R *res = (IP4R *) palloc(sizeof(IP4R));
    ip4r_union_internal(PG_GETARG_IP4R_P(0), PG_GETARG_IP4R_P(1), res);
    PG_RETURN_IP4R_P(res);
}

PG_FUNCTION_INFO_V1(ip4r_inter);
Datum
ip4r_inter(PG_FUNCTION_ARGS)
{
    IP4R *res = (IP4R *) palloc(sizeof(IP4R));
    if (ip4r_inter_internal(PG_GETARG_IP4R_P(0), PG_GETARG_IP4R_P(1), res))
    {
        PG_RETURN_IP4R_P(res);
    }
    pfree(res);
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(ip4r_size);
Datum
ip4r_size(PG_FUNCTION_ARGS)
{
    double size = ip4r_metric(PG_GETARG_IP4R_P(0));
    PG_RETURN_FLOAT8(size);
}

PG_FUNCTION_INFO_V1(ip4r_size_exact);
Datum
ip4r_size_exact(PG_FUNCTION_ARGS)
{
    int64 size = (int64) ip4r_metric(PG_GETARG_IP4R_P(0));
	PG_RETURN_DATUM(DirectFunctionCall1(int8_numeric, Int64GetDatumFast(size)));
}

PG_FUNCTION_INFO_V1(ip4r_prefixlen);
Datum
ip4r_prefixlen(PG_FUNCTION_ARGS)
{
    IP4R *ipr = PG_GETARG_IP4R_P(0);
	unsigned len = masklen(ipr->lower, ipr->upper);
	if (len <= 32)
		PG_RETURN_INT32((int32) len);
	PG_RETURN_NULL();
}


/*****************************************************************************
 *                                                 Btree functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(ip4r_cmp);
Datum
ip4r_cmp(PG_FUNCTION_ARGS)
{
    IP4R *a = PG_GETARG_IP4R_P(0);
    IP4R *b = PG_GETARG_IP4R_P(1);
    if (ip4r_lessthan(a,b))
        PG_RETURN_INT32(-1);
    if (ip4r_equal(a,b))
        PG_RETURN_INT32(0);
    PG_RETURN_INT32(1);
}

PG_FUNCTION_INFO_V1(ip4_cmp);
Datum
ip4_cmp(PG_FUNCTION_ARGS)
{
    IP4 a = PG_GETARG_IP4(0);
    IP4 b = PG_GETARG_IP4(1);
    if (ip4_lessthan(a,b))
        PG_RETURN_INT32(-1);
    if (ip4_equal(a,b))
        PG_RETURN_INT32(0);
    PG_RETURN_INT32(1);
}

/*
 * in_range(val ip4,base ip4,offset bigint,sub bool,less bool)
 * returns val CMP (base OP offset)
 * where CMP is <= if less, >= otherwise
 *   and OP is - if sub, + otherwise
 * We treat negative values of offset as special: they indicate
 * the (negation of) a cidr prefix length
 */
PG_FUNCTION_INFO_V1(ip4_in_range_bigint);
Datum
ip4_in_range_bigint(PG_FUNCTION_ARGS)
{
	IP4 val = PG_GETARG_IP4(0);
	IP4 base = PG_GETARG_IP4(1);
	int64 offset = PG_GETARG_INT64(2);
	bool sub = PG_GETARG_BOOL(3);
	bool less = PG_GETARG_BOOL(4);

	if (offset >= INT64CONST(0x100000000) || offset < -32)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE),
				 errmsg("invalid preceding or following size in window function"),
				 errdetail("Offset value " INT64_FORMAT " is outside the range -32 to 4294967295", offset)));

	if (offset < 0)
	{
		int bits = -offset;
		if (sub)
			base &= netmask(bits);
		else
			base |= hostmask(bits);
		if (less)
			PG_RETURN_BOOL(val <= base);
		else
			PG_RETURN_BOOL(val >= base);
	}
	else
	{
		/*
		 * val CMP (base OP offset) is equivalent to
		 * val - base CMP (OP offset), which avoids overflow.
		 */
		int64 delta = (int64)val - (int64)base;
		if (sub)
			offset = -offset;
		if (less)
			PG_RETURN_BOOL(delta <= offset);
		else
			PG_RETURN_BOOL(delta >= offset);
	}
}

PG_FUNCTION_INFO_V1(ip4_in_range_ip4);
Datum
ip4_in_range_ip4(PG_FUNCTION_ARGS)
{
	IP4 val = PG_GETARG_IP4(0);
	IP4 base = PG_GETARG_IP4(1);
	IP4 offset = PG_GETARG_IP4(2);
	bool sub = PG_GETARG_BOOL(3);
	bool less = PG_GETARG_BOOL(4);

	/*
	 * val CMP (base OP offset) is equivalent to
	 * val - base CMP (OP offset), which avoids overflow.
	 */
	int64 delta = (int64)val - (int64)base;
	int64 offs = (int64)offset;
	if (sub)
		offs = -offs;
	if (less)
		PG_RETURN_BOOL(delta <= offs);
	else
		PG_RETURN_BOOL(delta >= offs);
}


/*****************************************************************************
 *                                                 GiST functions
 *****************************************************************************/

/*
** GiST support methods
*/

Datum gip4r_consistent(PG_FUNCTION_ARGS);
Datum gip4r_compress(PG_FUNCTION_ARGS);
Datum gip4r_decompress(PG_FUNCTION_ARGS);
Datum gip4r_penalty(PG_FUNCTION_ARGS);
Datum gip4r_picksplit(PG_FUNCTION_ARGS);
Datum gip4r_union(PG_FUNCTION_ARGS);
Datum gip4r_same(PG_FUNCTION_ARGS);
Datum gip4r_fetch(PG_FUNCTION_ARGS);

static bool gip4r_leaf_consistent(IP4R * key, IP4R * query, StrategyNumber strategy);
static bool gip4r_internal_consistent(IP4R * key, IP4R * query, StrategyNumber strategy);

/*
** The GiST Consistent method for IP ranges
** Should return false if for all data items x below entry,
** the predicate x op query == false, where op is the oper
** corresponding to strategy in the pg_amop table.
*/
PG_FUNCTION_INFO_V1(gip4r_consistent);
Datum
gip4r_consistent(PG_FUNCTION_ARGS)
{
    GISTENTRY *entry = (GISTENTRY *) PG_GETARG_POINTER(0);
    IP4R *query = (IP4R *) PG_GETARG_POINTER(1);
    StrategyNumber strategy = (StrategyNumber) PG_GETARG_UINT16(2);
    bool *recheck = (bool *) PG_GETARG_POINTER(4);
    IP4R *key = (IP4R *) DatumGetPointer(entry->key);
    bool retval;

    /* recheck is never needed with this type */
    if (recheck)
        *recheck = false;

    /*
     * * if entry is not leaf, use gip4r_internal_consistent, * else use
     * gip4r_leaf_consistent
     */
    if (GIST_LEAF(entry))
        retval = gip4r_leaf_consistent(key, query, strategy);
    else
        retval = gip4r_internal_consistent(key, query, strategy);

    PG_RETURN_BOOL(retval);
}

/*
** The GiST Union method for IP ranges
** returns the minimal bounding IP4R that encloses all the entries in entryvec
*/
PG_FUNCTION_INFO_V1(gip4r_union);
Datum
gip4r_union(PG_FUNCTION_ARGS)
{
    GistEntryVector *entryvec = (GistEntryVector *) PG_GETARG_POINTER(0);
    int *sizep = (int *) PG_GETARG_POINTER(1);
    GISTENTRY *ent = GISTENTRYVEC(entryvec);

    int numranges, i;
    IP4R *out = (IP4R *) palloc(sizeof(IP4R));
    IP4R *tmp;

#ifdef GIST_DEBUG
    fprintf(stderr, "union\n");
#endif

    numranges = GISTENTRYCOUNT(entryvec);
    tmp = (IP4R *) DatumGetPointer(ent[0].key);
    *sizep = sizeof(IP4R);
    *out = *tmp;

    for (i = 1; i < numranges; i++)
    {
        tmp = (IP4R *) DatumGetPointer(ent[i].key);
        if (tmp->lower < out->lower)
            out->lower = tmp->lower;
        if (tmp->upper > out->upper)
            out->upper = tmp->upper;
    }

    PG_RETURN_IP4R_P(out);
}

/*
** GiST Compress and Decompress methods for IP ranges
** do not do anything.
*/
PG_FUNCTION_INFO_V1(gip4r_compress);
Datum
gip4r_compress(PG_FUNCTION_ARGS)
{
    PG_RETURN_POINTER(PG_GETARG_POINTER(0));
}

PG_FUNCTION_INFO_V1(gip4r_decompress);
Datum
gip4r_decompress(PG_FUNCTION_ARGS)
{
    PG_RETURN_POINTER(PG_GETARG_POINTER(0));
}

PG_FUNCTION_INFO_V1(gip4r_fetch);
Datum
gip4r_fetch(PG_FUNCTION_ARGS)
{
    PG_RETURN_POINTER(PG_GETARG_POINTER(0));
}

/*
** The GiST Penalty method for IP ranges
** As in the R-tree paper, we use change in area as our penalty metric
*/
PG_FUNCTION_INFO_V1(gip4r_penalty);
Datum
gip4r_penalty(PG_FUNCTION_ARGS)
{
    GISTENTRY *origentry = (GISTENTRY *) PG_GETARG_POINTER(0);
    GISTENTRY *newentry = (GISTENTRY *) PG_GETARG_POINTER(1);
    float *result = (float *) PG_GETARG_POINTER(2);
    IP4R *key;
    IP4R ud;
    float tmp1, tmp2;

    key = (IP4R *) DatumGetPointer(origentry->key);
    ud = *key;
    tmp2 = ip4r_metric(&ud);

    key = (IP4R *) DatumGetPointer(newentry->key);
    if (key->lower < ud.lower)
        ud.lower = key->lower;
    if (key->upper > ud.upper)
        ud.upper = key->upper;
    tmp1 = ip4r_metric(&ud);

    *result = tmp1 - tmp2;

#ifdef GIST_DEBUG
    fprintf(stderr, "penalty\n");
    fprintf(stderr, "\t%g\n", *result);
#endif

    PG_RETURN_POINTER(result);
}


/* Helper functions for picksplit. We might need to sort a list of
 * ranges by size; these are for that.
 */

struct gip4r_sort
{
    IP4R *key;
    OffsetNumber pos;
};

static int
gip4r_sort_compare(const void *a, const void *b)
{
    double sa = ip4r_metric(((struct gip4r_sort *)a)->key);
    double sb = ip4r_metric(((struct gip4r_sort *)b)->key);
    return (sa > sb) ? 1 : ((sa == sb) ? 0 : -1);
}

/*
** The GiST PickSplit method for IP ranges
** This is a linear-time algorithm based on a left/right split,
** based on the box functions in rtree_gist simplified to one
** dimension
*/
PG_FUNCTION_INFO_V1(gip4r_picksplit);
Datum
gip4r_picksplit(PG_FUNCTION_ARGS)
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
    IP4R pageunion;
    IP4R *cur;
    IP4R *unionL;
    IP4R *unionR;
    int posL = 0;
    int posR = 0;

    posL = posR = 0;
    maxoff = GISTENTRYCOUNT(entryvec) - 1;

    cur = (IP4R *) DatumGetPointer(ent[FirstOffsetNumber].key);
    pageunion = *cur;

    /* find MBR */
    for (i = OffsetNumberNext(FirstOffsetNumber); i <= maxoff; i = OffsetNumberNext(i))
    {
        cur = (IP4R *) DatumGetPointer(ent[i].key);
        if (allisequal == true
            && (pageunion.lower != cur->lower || pageunion.upper != cur->upper))
            allisequal = false;

        if (cur->lower < pageunion.lower)
            pageunion.lower = cur->lower;
        if (cur->upper > pageunion.upper)
            pageunion.upper = cur->upper;
    }

    nbytes = (maxoff + 2) * sizeof(OffsetNumber);
    listL = (OffsetNumber *) palloc(nbytes);
    listR = (OffsetNumber *) palloc(nbytes);
    unionL = (IP4R *) palloc(sizeof(IP4R));
    unionR = (IP4R *) palloc(sizeof(IP4R));
    v->spl_ldatum = PointerGetDatum(unionL);
    v->spl_rdatum = PointerGetDatum(unionR);
    v->spl_left = listL;
    v->spl_right = listR;

    if (allisequal)
    {
        cur = (IP4R *) DatumGetPointer(ent[OffsetNumberNext(FirstOffsetNumber)].key);
        if (ip4r_equal(cur, &pageunion))
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
    }

#define ADDLIST( list_, u_, pos_, num_ ) do { \
        if ( pos_ ) { \
                if ( (u_)->upper < (cur)->upper ) (u_)->upper = (cur)->upper; \
                if ( (u_)->lower > (cur)->lower ) (u_)->lower = (cur)->lower; \
        } else { \
                *(u_) = *(cur); \
        } \
        (list_)[(pos_)++] = (num_); \
} while(0)

    for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
    {
        cur = (IP4R *) DatumGetPointer(ent[i].key);
        if (cur->lower - pageunion.lower < pageunion.upper - cur->upper)
            ADDLIST(listL, unionL, posL, i);
        else
            ADDLIST(listR, unionR, posR, i);
    }

    /* bad disposition, sort by ascending size and resplit */
    if (posR == 0 || posL == 0)
    {
        struct gip4r_sort *arr = (struct gip4r_sort *)
            palloc(sizeof(struct gip4r_sort) * (maxoff + FirstOffsetNumber));

        for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
        {
            arr[i].key = (IP4R *) DatumGetPointer(ent[i].key);
            arr[i].pos = i;
        }

        qsort(arr + FirstOffsetNumber,
              maxoff - FirstOffsetNumber + 1,
              sizeof(struct gip4r_sort),
              gip4r_sort_compare);

        posL = posR = 0;
        for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
        {
            cur = arr[i].key;
            if (cur->lower - pageunion.lower < pageunion.upper - cur->upper)
                ADDLIST(listL, unionL, posL, arr[i].pos);
            else if (cur->lower - pageunion.lower == pageunion.upper - cur->upper)
            {
                if (posL > posR)
                    ADDLIST(listR, unionR, posR, arr[i].pos);
                else
                    ADDLIST(listL, unionL, posL, arr[i].pos);
            }
            else
                ADDLIST(listR, unionR, posR, arr[i].pos);
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
PG_FUNCTION_INFO_V1(gip4r_same);
Datum
gip4r_same(PG_FUNCTION_ARGS)
{
    IP4R *v1 = (IP4R *) PG_GETARG_POINTER(0);
    IP4R *v2 = (IP4R *) PG_GETARG_POINTER(1);
    bool *result = (bool *) PG_GETARG_POINTER(2);

    if (v1 && v2)
        *result = ip4r_equal(v1,v2);
    else
        *result = (v1 == NULL && v2 == NULL);

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
gip4r_leaf_consistent(IP4R * key,
                      IP4R * query,
                      StrategyNumber strategy)
{
#ifdef GIST_QUERY_DEBUG
    fprintf(stderr, "leaf_consistent, %d\n", strategy);
#endif

    switch (strategy)
    {
        case 1:   /* left contains right nonstrict */
            return ip4r_contains_internal(key, query, true);
        case 2:   /* left contained in right nonstrict */
            return ip4r_contains_internal(query, key, true);
        case 3:   /* left contains right strict */
            return ip4r_contains_internal(key, query, false);
        case 4:   /* left contained in right strict */
            return ip4r_contains_internal(query, key, false);
        case 5:   /* left overlaps right */
            return ip4r_overlaps_internal(key, query);
        case 6:   /* left equal right */
            return ip4r_equal(key, query);
        default:
            return false;
    }
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

bool
gip4r_internal_consistent(IP4R * key,
                          IP4R * query,
                          StrategyNumber strategy)
{
#ifdef GIST_QUERY_DEBUG
    fprintf(stderr, "internal_consistent, %d\n", strategy);
#endif

    switch (strategy)
    {
        case 2:   /* left contained in right nonstrict */
        case 4:   /* left contained in right strict */
        case 5:   /* left overlaps right */
            return ip4r_overlaps_internal(key, query);
        case 3:   /* left contains right strict */
            return ip4r_contains_internal(key, query, false);
        case 1:   /* left contains right nonstrict */
        case 6:   /* left equal right */
            return ip4r_contains_internal(key, query, true);
        default:
            return false;
    }
}

/* end */
