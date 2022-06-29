/* ipr_internal.h */
#ifndef IPR_INTERNAL_H
#define IPR_INTERNAL_H

#include "ipr.h"

#define IP4R_VERSION_STR "2.4.1"
#define IP4R_VERSION_NUM 20401

/* PG version dependencies */

#define INET_STRUCT_DATA(is_) ((inet_struct *)VARDATA_ANY(is_))

#define GISTENTRYCOUNT(v) ((v)->n)
#define GISTENTRYVEC(v) ((v)->vector)

/* hash_any_extended is new in pg11. On older pg, we don't care about what the
 * extended hash functions return, so just fake it.
 */

#if PG_VERSION_NUM < 110000

#ifndef ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE
#define ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE MAKE_SQLSTATE('2','2','0','1','3')
#endif

#ifndef DatumGetUInt64
#define DatumGetUInt64(d_) ((uint64) DatumGetInt64(d_))
#endif

#include "access/hash.h"

static inline
Datum hash_any_extended(register const unsigned char *k,
						register int keylen, uint64 seed)
{
	Datum d = hash_any(k, keylen);
	PG_RETURN_INT64((int64)(uint32) DatumGetInt32(d));
}

#endif

/* funcs */

Datum ip4_in(PG_FUNCTION_ARGS);
Datum ip4_out(PG_FUNCTION_ARGS);
Datum ip4_recv(PG_FUNCTION_ARGS);
Datum ip4_send(PG_FUNCTION_ARGS);
Datum ip4hash(PG_FUNCTION_ARGS);
Datum ip4_hash_extended(PG_FUNCTION_ARGS);
Datum ip4_cast_to_text(PG_FUNCTION_ARGS);
Datum ip4_cast_from_text(PG_FUNCTION_ARGS);
Datum ip4_cast_from_bit(PG_FUNCTION_ARGS);
Datum ip4_cast_to_bit(PG_FUNCTION_ARGS);
Datum ip4_cast_from_bytea(PG_FUNCTION_ARGS);
Datum ip4_cast_to_bytea(PG_FUNCTION_ARGS);
Datum ip4_cast_from_inet(PG_FUNCTION_ARGS);
Datum ip4_cast_to_cidr(PG_FUNCTION_ARGS);
Datum ip4_cast_to_bigint(PG_FUNCTION_ARGS);
Datum ip4_cast_to_numeric(PG_FUNCTION_ARGS);
Datum ip4_cast_from_bigint(PG_FUNCTION_ARGS);
Datum ip4_cast_from_numeric(PG_FUNCTION_ARGS);
Datum ip4_cast_to_double(PG_FUNCTION_ARGS);
Datum ip4_cast_from_double(PG_FUNCTION_ARGS);
Datum ip4r_in(PG_FUNCTION_ARGS);
Datum ip4r_out(PG_FUNCTION_ARGS);
Datum ip4r_recv(PG_FUNCTION_ARGS);
Datum ip4r_send(PG_FUNCTION_ARGS);
Datum ip4rhash(PG_FUNCTION_ARGS);
Datum ip4r_hash_extended(PG_FUNCTION_ARGS);
Datum ip4r_cast_to_text(PG_FUNCTION_ARGS);
Datum ip4r_cast_from_text(PG_FUNCTION_ARGS);
Datum ip4r_cast_from_bit(PG_FUNCTION_ARGS);
Datum ip4r_cast_to_bit(PG_FUNCTION_ARGS);
Datum ip4r_cast_from_cidr(PG_FUNCTION_ARGS);
Datum ip4r_cast_to_cidr(PG_FUNCTION_ARGS);
Datum ip4r_cast_from_ip4(PG_FUNCTION_ARGS);
Datum ip4r_from_ip4s(PG_FUNCTION_ARGS);
Datum ip4r_net_prefix(PG_FUNCTION_ARGS);
Datum ip4r_net_mask(PG_FUNCTION_ARGS);
Datum ip4r_lower(PG_FUNCTION_ARGS);
Datum ip4r_upper(PG_FUNCTION_ARGS);
Datum ip4r_is_cidr(PG_FUNCTION_ARGS);
Datum ip4r_cidr_split(PG_FUNCTION_ARGS);
Datum ip4_netmask(PG_FUNCTION_ARGS);
Datum ip4_net_lower(PG_FUNCTION_ARGS);
Datum ip4_net_upper(PG_FUNCTION_ARGS);
Datum ip4_plus_int(PG_FUNCTION_ARGS);
Datum ip4_plus_bigint(PG_FUNCTION_ARGS);
Datum ip4_plus_numeric(PG_FUNCTION_ARGS);
Datum ip4_minus_int(PG_FUNCTION_ARGS);
Datum ip4_minus_bigint(PG_FUNCTION_ARGS);
Datum ip4_minus_numeric(PG_FUNCTION_ARGS);
Datum ip4_minus_ip4(PG_FUNCTION_ARGS);
Datum ip4_and(PG_FUNCTION_ARGS);
Datum ip4_or(PG_FUNCTION_ARGS);
Datum ip4_xor(PG_FUNCTION_ARGS);
Datum ip4_not(PG_FUNCTION_ARGS);
Datum ip4_lt(PG_FUNCTION_ARGS);
Datum ip4_le(PG_FUNCTION_ARGS);
Datum ip4_gt(PG_FUNCTION_ARGS);
Datum ip4_ge(PG_FUNCTION_ARGS);
Datum ip4_eq(PG_FUNCTION_ARGS);
Datum ip4_neq(PG_FUNCTION_ARGS);
Datum ip4r_lt(PG_FUNCTION_ARGS);
Datum ip4r_le(PG_FUNCTION_ARGS);
Datum ip4r_gt(PG_FUNCTION_ARGS);
Datum ip4r_ge(PG_FUNCTION_ARGS);
Datum ip4r_eq(PG_FUNCTION_ARGS);
Datum ip4r_neq(PG_FUNCTION_ARGS);
Datum ip4r_overlaps(PG_FUNCTION_ARGS);
Datum ip4r_contains(PG_FUNCTION_ARGS);
Datum ip4r_contains_strict(PG_FUNCTION_ARGS);
Datum ip4r_contained_by(PG_FUNCTION_ARGS);
Datum ip4r_contained_by_strict(PG_FUNCTION_ARGS);
Datum ip4_contains(PG_FUNCTION_ARGS);
Datum ip4_contained_by(PG_FUNCTION_ARGS);
Datum ip4r_union(PG_FUNCTION_ARGS);
Datum ip4r_inter(PG_FUNCTION_ARGS);
Datum ip4r_size(PG_FUNCTION_ARGS);
Datum ip4r_size_exact(PG_FUNCTION_ARGS);
Datum ip4r_prefixlen(PG_FUNCTION_ARGS);
Datum ip4r_cmp(PG_FUNCTION_ARGS);
Datum ip4_cmp(PG_FUNCTION_ARGS);
Datum ip4_in_range_bigint(PG_FUNCTION_ARGS);
Datum ip4_in_range_ip4(PG_FUNCTION_ARGS);
Datum ip4r_left_of(PG_FUNCTION_ARGS);
Datum ip4r_right_of(PG_FUNCTION_ARGS);

Datum ip6_in(PG_FUNCTION_ARGS);
Datum ip6_out(PG_FUNCTION_ARGS);
Datum ip6_recv(PG_FUNCTION_ARGS);
Datum ip6_send(PG_FUNCTION_ARGS);
Datum ip6hash(PG_FUNCTION_ARGS);
Datum ip6_hash_extended(PG_FUNCTION_ARGS);
Datum ip6_cast_to_text(PG_FUNCTION_ARGS);
Datum ip6_cast_from_text(PG_FUNCTION_ARGS);
Datum ip6_cast_from_bit(PG_FUNCTION_ARGS);
Datum ip6_cast_to_bit(PG_FUNCTION_ARGS);
Datum ip6_cast_from_bytea(PG_FUNCTION_ARGS);
Datum ip6_cast_to_bytea(PG_FUNCTION_ARGS);
Datum ip6_cast_from_inet(PG_FUNCTION_ARGS);
Datum ip6_cast_to_cidr(PG_FUNCTION_ARGS);
Datum ip6_cast_to_numeric(PG_FUNCTION_ARGS);
Datum ip6_cast_from_numeric(PG_FUNCTION_ARGS);
Datum ip6r_in(PG_FUNCTION_ARGS);
Datum ip6r_out(PG_FUNCTION_ARGS);
Datum ip6r_recv(PG_FUNCTION_ARGS);
Datum ip6r_send(PG_FUNCTION_ARGS);
Datum ip6rhash(PG_FUNCTION_ARGS);
Datum ip6r_hash_extended(PG_FUNCTION_ARGS);
Datum ip6r_cast_to_text(PG_FUNCTION_ARGS);
Datum ip6r_cast_from_text(PG_FUNCTION_ARGS);
Datum ip6r_cast_from_bit(PG_FUNCTION_ARGS);
Datum ip6r_cast_to_bit(PG_FUNCTION_ARGS);
Datum ip6r_cast_from_cidr(PG_FUNCTION_ARGS);
Datum ip6r_cast_to_cidr(PG_FUNCTION_ARGS);
Datum ip6r_cast_from_ip6(PG_FUNCTION_ARGS);
Datum ip6r_from_ip6s(PG_FUNCTION_ARGS);
Datum ip6r_net_prefix(PG_FUNCTION_ARGS);
Datum ip6r_net_mask(PG_FUNCTION_ARGS);
Datum ip6r_lower(PG_FUNCTION_ARGS);
Datum ip6r_upper(PG_FUNCTION_ARGS);
Datum ip6r_is_cidr(PG_FUNCTION_ARGS);
Datum ip6r_cidr_split(PG_FUNCTION_ARGS);
Datum ip6_netmask(PG_FUNCTION_ARGS);
Datum ip6_net_lower(PG_FUNCTION_ARGS);
Datum ip6_net_upper(PG_FUNCTION_ARGS);
Datum ip6_plus_int(PG_FUNCTION_ARGS);
Datum ip6_plus_bigint(PG_FUNCTION_ARGS);
Datum ip6_plus_numeric(PG_FUNCTION_ARGS);
Datum ip6_minus_int(PG_FUNCTION_ARGS);
Datum ip6_minus_bigint(PG_FUNCTION_ARGS);
Datum ip6_minus_numeric(PG_FUNCTION_ARGS);
Datum ip6_minus_ip6(PG_FUNCTION_ARGS);
Datum ip6_and(PG_FUNCTION_ARGS);
Datum ip6_or(PG_FUNCTION_ARGS);
Datum ip6_xor(PG_FUNCTION_ARGS);
Datum ip6_not(PG_FUNCTION_ARGS);
Datum ip6_lt(PG_FUNCTION_ARGS);
Datum ip6_le(PG_FUNCTION_ARGS);
Datum ip6_gt(PG_FUNCTION_ARGS);
Datum ip6_ge(PG_FUNCTION_ARGS);
Datum ip6_eq(PG_FUNCTION_ARGS);
Datum ip6_neq(PG_FUNCTION_ARGS);
Datum ip6r_lt(PG_FUNCTION_ARGS);
Datum ip6r_le(PG_FUNCTION_ARGS);
Datum ip6r_gt(PG_FUNCTION_ARGS);
Datum ip6r_ge(PG_FUNCTION_ARGS);
Datum ip6r_eq(PG_FUNCTION_ARGS);
Datum ip6r_neq(PG_FUNCTION_ARGS);
Datum ip6r_overlaps(PG_FUNCTION_ARGS);
Datum ip6r_contains(PG_FUNCTION_ARGS);
Datum ip6r_contains_strict(PG_FUNCTION_ARGS);
Datum ip6r_contained_by(PG_FUNCTION_ARGS);
Datum ip6r_contained_by_strict(PG_FUNCTION_ARGS);
Datum ip6_contains(PG_FUNCTION_ARGS);
Datum ip6_contained_by(PG_FUNCTION_ARGS);
Datum ip6r_union(PG_FUNCTION_ARGS);
Datum ip6r_inter(PG_FUNCTION_ARGS);
Datum ip6r_size(PG_FUNCTION_ARGS);
Datum ip6r_size_exact(PG_FUNCTION_ARGS);
Datum ip6r_prefixlen(PG_FUNCTION_ARGS);
Datum ip6r_cmp(PG_FUNCTION_ARGS);
Datum ip6_cmp(PG_FUNCTION_ARGS);
Datum ip6_in_range_bigint(PG_FUNCTION_ARGS);
Datum ip6_in_range_ip6(PG_FUNCTION_ARGS);
#if 0
Datum ip6_in_range_numeric(PG_FUNCTION_ARGS);
#endif
Datum ip6r_left_of(PG_FUNCTION_ARGS);
Datum ip6r_right_of(PG_FUNCTION_ARGS);

Datum ipaddr_in(PG_FUNCTION_ARGS);
Datum ipaddr_out(PG_FUNCTION_ARGS);
Datum ipaddr_recv(PG_FUNCTION_ARGS);
Datum ipaddr_send(PG_FUNCTION_ARGS);
Datum ipaddr_hash(PG_FUNCTION_ARGS);
Datum ipaddr_hash_extended(PG_FUNCTION_ARGS);
Datum ipaddr_cast_to_text(PG_FUNCTION_ARGS);
Datum ipaddr_cast_from_text(PG_FUNCTION_ARGS);
Datum ipaddr_cast_from_bit(PG_FUNCTION_ARGS);
Datum ipaddr_cast_to_bit(PG_FUNCTION_ARGS);
Datum ipaddr_cast_from_bytea(PG_FUNCTION_ARGS);
Datum ipaddr_cast_to_bytea(PG_FUNCTION_ARGS);
Datum ipaddr_cast_from_inet(PG_FUNCTION_ARGS);
Datum ipaddr_cast_to_cidr(PG_FUNCTION_ARGS);
Datum ipaddr_cast_to_numeric(PG_FUNCTION_ARGS);
Datum ipaddr_cast_from_ip4(PG_FUNCTION_ARGS);
Datum ipaddr_cast_from_ip6(PG_FUNCTION_ARGS);
Datum ipaddr_cast_to_ip4(PG_FUNCTION_ARGS);
Datum ipaddr_cast_to_ip6(PG_FUNCTION_ARGS);
Datum ipaddr_net_lower(PG_FUNCTION_ARGS);
Datum ipaddr_net_upper(PG_FUNCTION_ARGS);
Datum ipaddr_family(PG_FUNCTION_ARGS);
Datum ipaddr_plus_int(PG_FUNCTION_ARGS);
Datum ipaddr_plus_bigint(PG_FUNCTION_ARGS);
Datum ipaddr_plus_numeric(PG_FUNCTION_ARGS);
Datum ipaddr_minus_int(PG_FUNCTION_ARGS);
Datum ipaddr_minus_bigint(PG_FUNCTION_ARGS);
Datum ipaddr_minus_numeric(PG_FUNCTION_ARGS);
Datum ipaddr_minus_ipaddr(PG_FUNCTION_ARGS);
Datum ipaddr_and(PG_FUNCTION_ARGS);
Datum ipaddr_or(PG_FUNCTION_ARGS);
Datum ipaddr_xor(PG_FUNCTION_ARGS);
Datum ipaddr_not(PG_FUNCTION_ARGS);
Datum ipaddr_lt(PG_FUNCTION_ARGS);
Datum ipaddr_le(PG_FUNCTION_ARGS);
Datum ipaddr_gt(PG_FUNCTION_ARGS);
Datum ipaddr_ge(PG_FUNCTION_ARGS);
Datum ipaddr_eq(PG_FUNCTION_ARGS);
Datum ipaddr_neq(PG_FUNCTION_ARGS);
Datum ipaddr_cmp(PG_FUNCTION_ARGS);

Datum iprange_in(PG_FUNCTION_ARGS);
Datum iprange_out(PG_FUNCTION_ARGS);
Datum iprange_recv(PG_FUNCTION_ARGS);
Datum iprange_send(PG_FUNCTION_ARGS);
Datum iprange_hash(PG_FUNCTION_ARGS);
Datum iprange_hash_new(PG_FUNCTION_ARGS);
Datum iprange_hash_extended(PG_FUNCTION_ARGS);
Datum iprange_cast_to_text(PG_FUNCTION_ARGS);
Datum iprange_cast_from_text(PG_FUNCTION_ARGS);
Datum iprange_cast_from_cidr(PG_FUNCTION_ARGS);
Datum iprange_cast_to_cidr(PG_FUNCTION_ARGS);
Datum iprange_cast_to_bit(PG_FUNCTION_ARGS);
Datum iprange_cast_from_ip4(PG_FUNCTION_ARGS);
Datum iprange_cast_from_ip6(PG_FUNCTION_ARGS);
Datum iprange_cast_from_ipaddr(PG_FUNCTION_ARGS);
Datum iprange_cast_from_ip4r(PG_FUNCTION_ARGS);
Datum iprange_cast_from_ip6r(PG_FUNCTION_ARGS);
Datum iprange_cast_to_ip4r(PG_FUNCTION_ARGS);
Datum iprange_cast_to_ip6r(PG_FUNCTION_ARGS);
Datum iprange_from_ip4s(PG_FUNCTION_ARGS);
Datum iprange_from_ip6s(PG_FUNCTION_ARGS);
Datum iprange_from_ipaddrs(PG_FUNCTION_ARGS);
Datum iprange_net_prefix_ip4(PG_FUNCTION_ARGS);
Datum iprange_net_prefix_ip6(PG_FUNCTION_ARGS);
Datum iprange_net_prefix(PG_FUNCTION_ARGS);
Datum iprange_net_mask_ip4(PG_FUNCTION_ARGS);
Datum iprange_net_mask_ip6(PG_FUNCTION_ARGS);
Datum iprange_net_mask(PG_FUNCTION_ARGS);
Datum iprange_lower(PG_FUNCTION_ARGS);
Datum iprange_upper(PG_FUNCTION_ARGS);
Datum iprange_is_cidr(PG_FUNCTION_ARGS);
Datum iprange_family(PG_FUNCTION_ARGS);
Datum iprange_cidr_split(PG_FUNCTION_ARGS);
Datum iprange_lt(PG_FUNCTION_ARGS);
Datum iprange_le(PG_FUNCTION_ARGS);
Datum iprange_gt(PG_FUNCTION_ARGS);
Datum iprange_ge(PG_FUNCTION_ARGS);
Datum iprange_eq(PG_FUNCTION_ARGS);
Datum iprange_neq(PG_FUNCTION_ARGS);
Datum iprange_overlaps(PG_FUNCTION_ARGS);
Datum iprange_contains(PG_FUNCTION_ARGS);
Datum iprange_contains_strict(PG_FUNCTION_ARGS);
Datum iprange_contained_by(PG_FUNCTION_ARGS);
Datum iprange_contained_by_strict(PG_FUNCTION_ARGS);
Datum iprange_contains_ip(PG_FUNCTION_ARGS);
Datum iprange_contains_ip4(PG_FUNCTION_ARGS);
Datum iprange_contains_ip6(PG_FUNCTION_ARGS);
Datum iprange_ip_contained_by(PG_FUNCTION_ARGS);
Datum iprange_ip4_contained_by(PG_FUNCTION_ARGS);
Datum iprange_ip6_contained_by(PG_FUNCTION_ARGS);
Datum iprange_union(PG_FUNCTION_ARGS);
Datum iprange_inter(PG_FUNCTION_ARGS);
Datum iprange_size(PG_FUNCTION_ARGS);
Datum iprange_size_exact(PG_FUNCTION_ARGS);
Datum iprange_prefixlen(PG_FUNCTION_ARGS);
Datum iprange_cmp(PG_FUNCTION_ARGS);

#endif
