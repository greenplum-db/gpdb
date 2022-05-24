-- ip4r extension

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION ip4r" to load this file. \quit

-- ----------------------------------------------------------------------
-- Type definitions

-- type creation is needlessly chatty

CREATE TYPE ip4;

CREATE FUNCTION ip4_in(cstring) RETURNS ip4 AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4_out(ip4) RETURNS cstring AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4_recv(internal) RETURNS ip4 AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4_send(ip4) RETURNS bytea AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE ip4 (
       INPUT = ip4_in, OUTPUT = ip4_out,
       RECEIVE = ip4_recv, SEND = ip4_send,
       INTERNALLENGTH = 4, ALIGNMENT = int4, PASSEDBYVALUE
);

COMMENT ON TYPE ip4 IS 'IPv4 address ''#.#.#.#''';

CREATE TYPE ip4r;

CREATE FUNCTION ip4r_in(cstring) RETURNS ip4r AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_out(ip4r) RETURNS cstring AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_recv(internal) RETURNS ip4r AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_send(ip4r) RETURNS bytea AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE ip4r (
       INPUT = ip4r_in, OUTPUT = ip4r_out,
       RECEIVE = ip4r_recv, SEND = ip4r_send,
       INTERNALLENGTH = 8, ALIGNMENT = int4
);

COMMENT ON TYPE ip4r IS 'IPv4 range ''#.#.#.#-#.#.#.#'' or ''#.#.#.#/#'' or ''#.#.#.#''';

CREATE TYPE ip6;

CREATE FUNCTION ip6_in(cstring) RETURNS ip6 AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_out(ip6) RETURNS cstring AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_recv(internal) RETURNS ip6 AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_send(ip6) RETURNS bytea AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE ip6 (
       INPUT = ip6_in, OUTPUT = ip6_out,
       RECEIVE = ip6_recv, SEND = ip6_send,
       INTERNALLENGTH = 16, ALIGNMENT = double
);

COMMENT ON TYPE ip6 IS 'IPv6 address ''xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx''';

CREATE TYPE ip6r;

CREATE FUNCTION ip6r_in(cstring) RETURNS ip6r AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_out(ip6r) RETURNS cstring AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_recv(internal) RETURNS ip6r AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_send(ip6r) RETURNS bytea AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE ip6r (
       INPUT = ip6r_in, OUTPUT = ip6r_out,
       RECEIVE = ip6r_recv, SEND = ip6r_send,
       INTERNALLENGTH = 32, ALIGNMENT = double
);

COMMENT ON TYPE ip6r IS 'IPv6 range ''#-#'' or ''#/#'' or ''#''';

CREATE TYPE ipaddress;

CREATE FUNCTION ipaddress_in(cstring) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_in' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_out(ipaddress) RETURNS cstring AS 'MODULE_PATHNAME','ipaddr_out' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_recv(internal) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_recv' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_send(ipaddress) RETURNS bytea AS 'MODULE_PATHNAME','ipaddr_send' LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE ipaddress (
       INPUT = ipaddress_in, OUTPUT = ipaddress_out,
       RECEIVE = ipaddress_recv, SEND = ipaddress_send,
       INTERNALLENGTH = VARIABLE, ALIGNMENT = int4, STORAGE = main
);

COMMENT ON TYPE ipaddress IS 'IPv4 or IPv6 address';

CREATE TYPE iprange;

CREATE FUNCTION iprange_in(cstring) RETURNS iprange AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_out(iprange) RETURNS cstring AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_recv(internal) RETURNS iprange AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_send(iprange) RETURNS bytea AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE iprange (
       INPUT = iprange_in, OUTPUT = iprange_out,
       RECEIVE = iprange_recv, SEND = iprange_send,
       INTERNALLENGTH = VARIABLE, ALIGNMENT = int4, STORAGE = main
);

COMMENT ON TYPE iprange IS 'IPv4 or IPv6 range';

-- ----------------------------------------------------------------------
-- Cast functions (inward)

CREATE FUNCTION ip4(bigint) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4(double precision) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_double' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4(numeric) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_numeric' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4(inet) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_inet' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4(text) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_text' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4(bit) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4(varbit) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4(bytea) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_bytea' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4(ipaddress) RETURNS ip4 AS 'MODULE_PATHNAME','ipaddr_cast_to_ip4' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip6(numeric) RETURNS ip6 AS 'MODULE_PATHNAME','ip6_cast_from_numeric' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6(inet) RETURNS ip6 AS 'MODULE_PATHNAME','ip6_cast_from_inet' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6(text) RETURNS ip6 AS 'MODULE_PATHNAME','ip6_cast_from_text' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6(bit) RETURNS ip6 AS 'MODULE_PATHNAME','ip6_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6(varbit) RETURNS ip6 AS 'MODULE_PATHNAME','ip6_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6(bytea) RETURNS ip6 AS 'MODULE_PATHNAME','ip6_cast_from_bytea' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6(ipaddress) RETURNS ip6 AS 'MODULE_PATHNAME','ipaddr_cast_to_ip6' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ipaddress(inet) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_cast_from_inet' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress(ip4) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_cast_from_ip4' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress(ip6) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_cast_from_ip6' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress(text) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_cast_from_text' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress(bit) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress(varbit) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress(bytea) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_cast_from_bytea' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip4r(cidr) RETURNS ip4r AS 'MODULE_PATHNAME','ip4r_cast_from_cidr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r(ip4) RETURNS ip4r AS 'MODULE_PATHNAME','ip4r_cast_from_ip4' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r(text) RETURNS ip4r AS 'MODULE_PATHNAME','ip4r_cast_from_text' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r(varbit) RETURNS ip4r AS 'MODULE_PATHNAME','ip4r_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r(iprange) RETURNS ip4r AS 'MODULE_PATHNAME','iprange_cast_to_ip4r' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip6r(cidr) RETURNS ip6r AS 'MODULE_PATHNAME','ip6r_cast_from_cidr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r(ip6) RETURNS ip6r AS 'MODULE_PATHNAME','ip6r_cast_from_ip6' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r(text) RETURNS ip6r AS 'MODULE_PATHNAME','ip6r_cast_from_text' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r(varbit) RETURNS ip6r AS 'MODULE_PATHNAME','ip6r_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r(iprange) RETURNS ip6r AS 'MODULE_PATHNAME','iprange_cast_to_ip6r' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION iprange(cidr) RETURNS iprange AS 'MODULE_PATHNAME','iprange_cast_from_cidr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange(ip4) RETURNS iprange AS 'MODULE_PATHNAME','iprange_cast_from_ip4' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange(ip6) RETURNS iprange AS 'MODULE_PATHNAME','iprange_cast_from_ip6' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange(ip4r) RETURNS iprange AS 'MODULE_PATHNAME','iprange_cast_from_ip4r' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange(ip6r) RETURNS iprange AS 'MODULE_PATHNAME','iprange_cast_from_ip6r' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange(ipaddress) RETURNS iprange AS 'MODULE_PATHNAME','iprange_cast_from_ipaddr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange(text) RETURNS iprange AS 'MODULE_PATHNAME','iprange_cast_from_text' LANGUAGE C IMMUTABLE STRICT;

-- ----------------------------------------------------------------------
-- Cast functions (outward)

CREATE FUNCTION cidr(ip4) RETURNS cidr AS 'MODULE_PATHNAME','ip4_cast_to_cidr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION cidr(ip4r) RETURNS cidr AS 'MODULE_PATHNAME','ip4r_cast_to_cidr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION cidr(ip6) RETURNS cidr AS 'MODULE_PATHNAME','ip6_cast_to_cidr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION cidr(ip6r) RETURNS cidr AS 'MODULE_PATHNAME','ip6r_cast_to_cidr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION cidr(ipaddress) RETURNS cidr AS 'MODULE_PATHNAME','ipaddr_cast_to_cidr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION cidr(iprange) RETURNS cidr AS 'MODULE_PATHNAME','iprange_cast_to_cidr' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION text(ip4) RETURNS text AS 'MODULE_PATHNAME','ip4_cast_to_text' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION text(ip4r) RETURNS text AS 'MODULE_PATHNAME','ip4r_cast_to_text' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION text(ip6) RETURNS text AS 'MODULE_PATHNAME','ip6_cast_to_text' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION text(ip6r) RETURNS text AS 'MODULE_PATHNAME','ip6r_cast_to_text' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION text(ipaddress) RETURNS text AS 'MODULE_PATHNAME','ipaddr_cast_to_text' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION text(iprange) RETURNS text AS 'MODULE_PATHNAME','iprange_cast_to_text' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION to_bigint(ip4) RETURNS bigint AS 'MODULE_PATHNAME','ip4_cast_to_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_double(ip4) RETURNS double precision AS 'MODULE_PATHNAME','ip4_cast_to_double' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_numeric(ip4) RETURNS numeric AS 'MODULE_PATHNAME','ip4_cast_to_numeric' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_numeric(ip6) RETURNS numeric AS 'MODULE_PATHNAME','ip6_cast_to_numeric' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_numeric(ipaddress) RETURNS numeric AS 'MODULE_PATHNAME','ipaddr_cast_to_numeric' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(ip4) RETURNS varbit AS 'MODULE_PATHNAME','ip4_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(ip6) RETURNS varbit AS 'MODULE_PATHNAME','ip6_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(ipaddress) RETURNS varbit AS 'MODULE_PATHNAME','ipaddr_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(ip4r) RETURNS varbit AS 'MODULE_PATHNAME','ip4r_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(ip6r) RETURNS varbit AS 'MODULE_PATHNAME','ip6r_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(iprange) RETURNS varbit AS 'MODULE_PATHNAME','iprange_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bytea(ip4) RETURNS bytea AS 'MODULE_PATHNAME','ip4_cast_to_bytea' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bytea(ip6) RETURNS bytea AS 'MODULE_PATHNAME','ip6_cast_to_bytea' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bytea(ipaddress) RETURNS bytea AS 'MODULE_PATHNAME','ipaddr_cast_to_bytea' LANGUAGE C IMMUTABLE STRICT;

-- ----------------------------------------------------------------------
-- Cast definitions (outward)

-- all these are explicit, with the exception of casts from single-address
-- types to "cidr", which we make assignment casts since they are lossless
-- and preserve general semantics.

CREATE CAST (ip4 as bigint) WITH FUNCTION to_bigint(ip4);
CREATE CAST (ip4 as double precision) WITH FUNCTION to_double(ip4);
CREATE CAST (ip4 as numeric) WITH FUNCTION to_numeric(ip4);
CREATE CAST (ip4 as varbit) WITH FUNCTION to_bit(ip4);
CREATE CAST (ip4 as bytea) WITH FUNCTION to_bytea(ip4);
CREATE CAST (ip4 as text) WITH FUNCTION text(ip4);

CREATE CAST (ip4 as cidr) WITH FUNCTION cidr(ip4) AS ASSIGNMENT;

CREATE CAST (ip4r as cidr) WITH FUNCTION cidr(ip4r);
CREATE CAST (ip4r as varbit) WITH FUNCTION to_bit(ip4r);
CREATE CAST (ip4r as text) WITH FUNCTION text(ip4r);

CREATE CAST (ip6 as numeric) WITH FUNCTION to_numeric(ip6);
CREATE CAST (ip6 as varbit) WITH FUNCTION to_bit(ip6);
CREATE CAST (ip6 as bytea) WITH FUNCTION to_bytea(ip6);
CREATE CAST (ip6 as text) WITH FUNCTION text(ip6);

CREATE CAST (ip6 as cidr) WITH FUNCTION cidr(ip6) AS ASSIGNMENT;

CREATE CAST (ip6r as cidr) WITH FUNCTION cidr(ip6r);
CREATE CAST (ip6r as varbit) WITH FUNCTION to_bit(ip6r);
CREATE CAST (ip6r as text) WITH FUNCTION text(ip6r);

CREATE CAST (ipaddress as numeric) WITH FUNCTION to_numeric(ipaddress);
CREATE CAST (ipaddress as varbit) WITH FUNCTION to_bit(ipaddress);
CREATE CAST (ipaddress as bytea) WITH FUNCTION to_bytea(ipaddress);
CREATE CAST (ipaddress as text) WITH FUNCTION text(ipaddress);

CREATE CAST (ipaddress as cidr) WITH FUNCTION cidr(ipaddress) AS ASSIGNMENT;

CREATE CAST (iprange as cidr) WITH FUNCTION cidr(iprange);
CREATE CAST (iprange as varbit) WITH FUNCTION to_bit(iprange);
CREATE CAST (iprange as text) WITH FUNCTION text(iprange);

-- ----------------------------------------------------------------------
-- Cast definitions (inward)

-- these are explicit except for casts from inet/cidr types. Even though
-- such casts are lossy for inet, since the masklen isn't preserved, the
-- semantics and common usage are enough to justify an assignment cast.

CREATE CAST (text as ip4) WITH FUNCTION ip4(text);
CREATE CAST (text as ip4r) WITH FUNCTION ip4r(text);
CREATE CAST (text as ip6) WITH FUNCTION ip6(text);
CREATE CAST (text as ip6r) WITH FUNCTION ip6r(text);
CREATE CAST (text as ipaddress) WITH FUNCTION ipaddress(text);
CREATE CAST (text as iprange) WITH FUNCTION iprange(text);

CREATE CAST (bigint as ip4) WITH FUNCTION ip4(bigint);
CREATE CAST (double precision as ip4) WITH FUNCTION ip4(double precision);

CREATE CAST (numeric as ip4) WITH FUNCTION ip4(numeric);
CREATE CAST (numeric as ip6) WITH FUNCTION ip6(numeric);

CREATE CAST (bit as ip4) WITH FUNCTION ip4(bit);
CREATE CAST (bit as ip6) WITH FUNCTION ip6(bit);
CREATE CAST (bit as ipaddress) WITH FUNCTION ipaddress(bit);

CREATE CAST (varbit as ip4) WITH FUNCTION ip4(bit);
CREATE CAST (varbit as ip6) WITH FUNCTION ip6(bit);
CREATE CAST (varbit as ipaddress) WITH FUNCTION ipaddress(varbit);
CREATE CAST (varbit as ip4r) WITH FUNCTION ip4r(varbit);
CREATE CAST (varbit as ip6r) WITH FUNCTION ip6r(varbit);

CREATE CAST (bytea as ip4) WITH FUNCTION ip4(bytea);
CREATE CAST (bytea as ip6) WITH FUNCTION ip6(bytea);
CREATE CAST (bytea as ipaddress) WITH FUNCTION ipaddress(bytea);

CREATE CAST (cidr as ip4r) WITH FUNCTION ip4r(cidr) AS ASSIGNMENT;
CREATE CAST (cidr as ip6r) WITH FUNCTION ip6r(cidr) AS ASSIGNMENT;
CREATE CAST (cidr as iprange) WITH FUNCTION iprange(cidr) AS ASSIGNMENT;

CREATE CAST (inet as ip4) WITH FUNCTION ip4(inet) AS ASSIGNMENT;
CREATE CAST (inet as ip6) WITH FUNCTION ip6(inet) AS ASSIGNMENT;
CREATE CAST (inet as ipaddress) WITH FUNCTION ipaddress(inet) AS ASSIGNMENT;

-- ----------------------------------------------------------------------
-- Cast definitions (cross-type)

-- the lossless "upward" casts are made implict. Downward casts are now
-- assignment casts.

CREATE CAST (ip4 as ip4r) WITH FUNCTION ip4r(ip4) AS IMPLICIT;
CREATE CAST (ip4 as ipaddress) WITH FUNCTION ipaddress(ip4) AS IMPLICIT;
CREATE CAST (ip4 as iprange) WITH FUNCTION iprange(ip4) AS IMPLICIT;
CREATE CAST (ip4r as iprange) WITH FUNCTION iprange(ip4r) AS IMPLICIT;

CREATE CAST (ip6 as ip6r) WITH FUNCTION ip6r(ip6) AS IMPLICIT;
CREATE CAST (ip6 as ipaddress) WITH FUNCTION ipaddress(ip6) AS IMPLICIT;
CREATE CAST (ip6 as iprange) WITH FUNCTION iprange(ip6) AS IMPLICIT;
CREATE CAST (ip6r as iprange) WITH FUNCTION iprange(ip6r) AS IMPLICIT;

CREATE CAST (ipaddress as iprange) WITH FUNCTION iprange(ipaddress) AS IMPLICIT;

CREATE CAST (ipaddress as ip4) WITH FUNCTION ip4(ipaddress) AS ASSIGNMENT;
CREATE CAST (ipaddress as ip6) WITH FUNCTION ip6(ipaddress) AS ASSIGNMENT;

CREATE CAST (iprange as ip4r) WITH FUNCTION ip4r(iprange) AS ASSIGNMENT;
CREATE CAST (iprange as ip6r) WITH FUNCTION ip6r(iprange) AS ASSIGNMENT;

-- ----------------------------------------------------------------------
-- Constructor functions

CREATE FUNCTION ip4r(ip4,ip4) RETURNS ip4r AS 'MODULE_PATHNAME', 'ip4r_from_ip4s' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r(ip6,ip6) RETURNS ip6r AS 'MODULE_PATHNAME', 'ip6r_from_ip6s' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange(ip4,ip4) RETURNS iprange AS 'MODULE_PATHNAME', 'iprange_from_ip4s' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange(ip6,ip6) RETURNS iprange AS 'MODULE_PATHNAME', 'iprange_from_ip6s' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange(ipaddress,ipaddress) RETURNS iprange AS 'MODULE_PATHNAME', 'iprange_from_ipaddrs' LANGUAGE C IMMUTABLE STRICT;

-- ----------------------------------------------------------------------
-- Utility functions (no operator equivalent)

CREATE FUNCTION family(ip4) RETURNS integer AS $f$ select 4; $f$ LANGUAGE SQL IMMUTABLE;
CREATE FUNCTION family(ip4r) RETURNS integer AS $f$ select 4; $f$ LANGUAGE SQL IMMUTABLE;
CREATE FUNCTION family(ip6) RETURNS integer AS $f$ select 6; $f$ LANGUAGE SQL IMMUTABLE;
CREATE FUNCTION family(ip6r) RETURNS integer AS $f$ select 6; $f$ LANGUAGE SQL IMMUTABLE;
CREATE FUNCTION family(ipaddress) RETURNS integer AS 'MODULE_PATHNAME', 'ipaddr_family' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION family(iprange) RETURNS integer AS 'MODULE_PATHNAME', 'iprange_family' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip4_netmask(integer) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_netmask' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_netmask(integer) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_netmask' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION is_cidr(ip4r) RETURNS boolean AS 'MODULE_PATHNAME', 'ip4r_is_cidr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION is_cidr(ip6r) RETURNS boolean AS 'MODULE_PATHNAME', 'ip6r_is_cidr' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION is_cidr(iprange) RETURNS boolean AS 'MODULE_PATHNAME', 'iprange_is_cidr' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION masklen(ip4r) RETURNS integer AS 'MODULE_PATHNAME','ip4r_prefixlen' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION masklen(ip6r) RETURNS integer AS 'MODULE_PATHNAME','ip6r_prefixlen' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION masklen(iprange) RETURNS integer AS 'MODULE_PATHNAME','iprange_prefixlen' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION lower(ip4r) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4r_lower' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION lower(ip6r) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6r_lower' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION lower(iprange) RETURNS ipaddress AS 'MODULE_PATHNAME', 'iprange_lower' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION upper(ip4r) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4r_upper' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION upper(ip6r) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6r_upper' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION upper(iprange) RETURNS ipaddress AS 'MODULE_PATHNAME', 'iprange_upper' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip4_net_lower(ip4,integer) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_net_lower' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_net_lower(ip6,integer) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_net_lower' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_net_lower(ipaddress,integer) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_net_lower' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip4_net_upper(ip4,integer) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_net_upper' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_net_upper(ip6,integer) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_net_upper' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_net_upper(ipaddress,integer) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_net_upper' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip4r_union(ip4r, ip4r) RETURNS ip4r AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_union(ip6r, ip6r) RETURNS ip6r AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_union(iprange, iprange) RETURNS iprange AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip4r_inter(ip4r,ip4r) RETURNS ip4r AS 'MODULE_PATHNAME','ip4r_inter' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_inter(ip6r,ip6r) RETURNS ip6r AS 'MODULE_PATHNAME','ip6r_inter' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_inter(iprange,iprange) RETURNS iprange AS 'MODULE_PATHNAME','iprange_inter' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION cidr_split(ip4r) RETURNS SETOF ip4r AS 'MODULE_PATHNAME','ip4r_cidr_split' LANGUAGE C IMMUTABLE STRICT ROWS 10;
CREATE FUNCTION cidr_split(ip6r) RETURNS SETOF ip6r AS 'MODULE_PATHNAME','ip6r_cidr_split' LANGUAGE C IMMUTABLE STRICT ROWS 50;
CREATE FUNCTION cidr_split(iprange) RETURNS SETOF iprange AS 'MODULE_PATHNAME','iprange_cidr_split' LANGUAGE C IMMUTABLE STRICT ROWS 30;

-- ----------------------------------------------------------------------
-- Functions with operator equivalents

-- it's intended that either the function form or the operator form be used,
-- as desired.

-- (ip / len) or (ip / mask)

CREATE FUNCTION ip4r_net_mask(ip4,ip4) RETURNS ip4r AS 'MODULE_PATHNAME','ip4r_net_mask' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_net_mask(ip6,ip6) RETURNS ip6r AS 'MODULE_PATHNAME','ip6r_net_mask' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_net_mask(ip4,ip4) RETURNS iprange AS 'MODULE_PATHNAME','iprange_net_mask_ip4' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_net_mask(ip6,ip6) RETURNS iprange AS 'MODULE_PATHNAME','iprange_net_mask_ip6' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_net_mask(ipaddress,ipaddress) RETURNS iprange AS 'MODULE_PATHNAME','iprange_net_mask' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip4r_net_prefix(ip4,integer) RETURNS ip4r AS 'MODULE_PATHNAME','ip4r_net_prefix' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_net_prefix(ip6,integer) RETURNS ip6r AS 'MODULE_PATHNAME','ip6r_net_prefix' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_net_prefix(ip4,integer) RETURNS iprange AS 'MODULE_PATHNAME','iprange_net_prefix_ip4' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_net_prefix(ip6,integer) RETURNS iprange AS 'MODULE_PATHNAME','iprange_net_prefix_ip6' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_net_prefix(ipaddress,integer) RETURNS iprange AS 'MODULE_PATHNAME','iprange_net_prefix' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR / ( LEFTARG = ip4,       RIGHTARG = ip4,       PROCEDURE = ip4r_net_mask      );
CREATE OPERATOR / ( LEFTARG = ip6,       RIGHTARG = ip6,       PROCEDURE = ip6r_net_mask      );
CREATE OPERATOR / ( LEFTARG = ipaddress, RIGHTARG = ipaddress, PROCEDURE = iprange_net_mask   );
CREATE OPERATOR / ( LEFTARG = ip4,       RIGHTARG = integer,   PROCEDURE = ip4r_net_prefix    );
CREATE OPERATOR / ( LEFTARG = ip6,       RIGHTARG = integer,   PROCEDURE = ip6r_net_prefix    );
CREATE OPERATOR / ( LEFTARG = ipaddress, RIGHTARG = integer,   PROCEDURE = iprange_net_prefix );

-- @ ipr  (approximate size) or  @@ ipr  (exact size)

CREATE FUNCTION ip4r_size(ip4r) RETURNS double precision AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_size(ip6r) RETURNS double precision AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_size(iprange) RETURNS double precision AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip4r_size_exact(ip4r) RETURNS numeric AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_size_exact(ip6r) RETURNS numeric AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_size_exact(iprange) RETURNS numeric AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR @ ( RIGHTARG = ip4r,    PROCEDURE = ip4r_size    );
CREATE OPERATOR @ ( RIGHTARG = ip6r,    PROCEDURE = ip6r_size    );
CREATE OPERATOR @ ( RIGHTARG = iprange, PROCEDURE = iprange_size );

CREATE OPERATOR @@ ( RIGHTARG = ip4r,    PROCEDURE = ip4r_size_exact    );
CREATE OPERATOR @@ ( RIGHTARG = ip6r,    PROCEDURE = ip6r_size_exact    );
CREATE OPERATOR @@ ( RIGHTARG = iprange, PROCEDURE = iprange_size_exact );

-- ----------------------------------------------------------------------
-- Operators

-- the function forms of these aren't intended for general use

-- bitwise ops: and (a & b), or (a | b), xor (a # b), not (~a)

CREATE FUNCTION ip4_and(ip4,ip4) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_and' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_and(ip6,ip6) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_and' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_and(ipaddress,ipaddress) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_and' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR & ( LEFTARG = ip4,       RIGHTARG = ip4,        PROCEDURE = ip4_and );
CREATE OPERATOR & ( LEFTARG = ip6,       RIGHTARG = ip6,        PROCEDURE = ip6_and );
CREATE OPERATOR & ( LEFTARG = ipaddress, RIGHTARG = ipaddress,  PROCEDURE = ipaddress_and );

CREATE FUNCTION ip4_or(ip4,ip4) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_or' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_or(ip6,ip6) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_or' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_or(ipaddress,ipaddress) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_or' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR | ( LEFTARG = ip4,       RIGHTARG = ip4,        PROCEDURE = ip4_or );
CREATE OPERATOR | ( LEFTARG = ip6,       RIGHTARG = ip6,        PROCEDURE = ip6_or );
CREATE OPERATOR | ( LEFTARG = ipaddress, RIGHTARG = ipaddress,  PROCEDURE = ipaddress_or );

CREATE FUNCTION ip4_not(ip4) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_not' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_not(ip6) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_not' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_not(ipaddress) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_not' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR ~ ( RIGHTARG = ip4,       PROCEDURE = ip4_not );
CREATE OPERATOR ~ ( RIGHTARG = ip6,       PROCEDURE = ip6_not );
CREATE OPERATOR ~ ( RIGHTARG = ipaddress, PROCEDURE = ipaddress_not );

CREATE FUNCTION ip4_xor(ip4,ip4) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_xor' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_xor(ip6,ip6) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_xor' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_xor(ipaddress,ipaddress) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_xor' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR # ( LEFTARG = ip4,       RIGHTARG = ip4,        PROCEDURE = ip4_xor );
CREATE OPERATOR # ( LEFTARG = ip6,       RIGHTARG = ip6,        PROCEDURE = ip6_xor );
CREATE OPERATOR # ( LEFTARG = ipaddress, RIGHTARG = ipaddress,  PROCEDURE = ipaddress_xor );

-- arithmetic ops: (ip + n), (ip - n), (ip - ip) where n is a numeric or integer type

CREATE FUNCTION ip4_plus_bigint(ip4,bigint) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_plus_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4_plus_int(ip4,integer) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_plus_int' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4_plus_numeric(ip4,numeric) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_plus_numeric' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR + ( LEFTARG = ip4, RIGHTARG = bigint,  PROCEDURE = ip4_plus_bigint );
CREATE OPERATOR + ( LEFTARG = ip4, RIGHTARG = integer, PROCEDURE = ip4_plus_int );
CREATE OPERATOR + ( LEFTARG = ip4, RIGHTARG = numeric, PROCEDURE = ip4_plus_numeric );

CREATE FUNCTION ip6_plus_bigint(ip6,bigint) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_plus_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_plus_int(ip6,integer) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_plus_int' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_plus_numeric(ip6,numeric) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_plus_numeric' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR + ( LEFTARG = ip6, RIGHTARG = bigint,  PROCEDURE = ip6_plus_bigint );
CREATE OPERATOR + ( LEFTARG = ip6, RIGHTARG = integer, PROCEDURE = ip6_plus_int );
CREATE OPERATOR + ( LEFTARG = ip6, RIGHTARG = numeric, PROCEDURE = ip6_plus_numeric );

CREATE FUNCTION ipaddress_plus_bigint(ipaddress,bigint) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_plus_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_plus_int(ipaddress,integer) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_plus_int' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_plus_numeric(ipaddress,numeric) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_plus_numeric' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR + ( LEFTARG = ipaddress, RIGHTARG = bigint,  PROCEDURE = ipaddress_plus_bigint );
CREATE OPERATOR + ( LEFTARG = ipaddress, RIGHTARG = integer, PROCEDURE = ipaddress_plus_int );
CREATE OPERATOR + ( LEFTARG = ipaddress, RIGHTARG = numeric, PROCEDURE = ipaddress_plus_numeric );

CREATE FUNCTION ip4_minus_bigint(ip4,bigint) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_minus_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4_minus_int(ip4,integer) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_minus_int' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4_minus_numeric(ip4,numeric) RETURNS ip4 AS 'MODULE_PATHNAME', 'ip4_minus_numeric' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR - ( LEFTARG = ip4, RIGHTARG = bigint,  PROCEDURE = ip4_minus_bigint );
CREATE OPERATOR - ( LEFTARG = ip4, RIGHTARG = integer, PROCEDURE = ip4_minus_int );
CREATE OPERATOR - ( LEFTARG = ip4, RIGHTARG = numeric, PROCEDURE = ip4_minus_numeric );

CREATE FUNCTION ip6_minus_bigint(ip6,bigint) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_minus_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_minus_int(ip6,integer) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_minus_int' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_minus_numeric(ip6,numeric) RETURNS ip6 AS 'MODULE_PATHNAME', 'ip6_minus_numeric' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR - ( LEFTARG = ip6, RIGHTARG = bigint,  PROCEDURE = ip6_minus_bigint );
CREATE OPERATOR - ( LEFTARG = ip6, RIGHTARG = integer, PROCEDURE = ip6_minus_int );
CREATE OPERATOR - ( LEFTARG = ip6, RIGHTARG = numeric, PROCEDURE = ip6_minus_numeric );

CREATE FUNCTION ipaddress_minus_bigint(ipaddress,bigint) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_minus_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_minus_int(ipaddress,integer) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_minus_int' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_minus_numeric(ipaddress,numeric) RETURNS ipaddress AS 'MODULE_PATHNAME', 'ipaddr_minus_numeric' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR - ( LEFTARG = ipaddress, RIGHTARG = bigint,  PROCEDURE = ipaddress_minus_bigint );
CREATE OPERATOR - ( LEFTARG = ipaddress, RIGHTARG = integer, PROCEDURE = ipaddress_minus_int );
CREATE OPERATOR - ( LEFTARG = ipaddress, RIGHTARG = numeric, PROCEDURE = ipaddress_minus_numeric );

CREATE FUNCTION ip4_minus_ip4(ip4,ip4) RETURNS bigint AS 'MODULE_PATHNAME', 'ip4_minus_ip4' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_minus_ip6(ip6,ip6) RETURNS numeric AS 'MODULE_PATHNAME', 'ip6_minus_ip6' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_minus_ipaddress(ipaddress,ipaddress) RETURNS numeric AS 'MODULE_PATHNAME', 'ipaddr_minus_ipaddr' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR - ( LEFTARG = ip4,       RIGHTARG = ip4,       PROCEDURE = ip4_minus_ip4 );
CREATE OPERATOR - ( LEFTARG = ip6,       RIGHTARG = ip6,       PROCEDURE = ip6_minus_ip6 );
CREATE OPERATOR - ( LEFTARG = ipaddress, RIGHTARG = ipaddress, PROCEDURE = ipaddress_minus_ipaddress );

-- containment predicates: (a >>= b), (a >> b), (a <<= b), (a << b), (a && b)

CREATE FUNCTION ip4r_contained_by(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_contained_by(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_contained_by(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR <<= ( LEFTARG = ip4r,    RIGHTARG = ip4r,    PROCEDURE = ip4r_contained_by,    COMMUTATOR = '>>=', RESTRICT = contsel, JOIN = contjoinsel );
CREATE OPERATOR <<= ( LEFTARG = ip6r,    RIGHTARG = ip6r,    PROCEDURE = ip6r_contained_by,    COMMUTATOR = '>>=', RESTRICT = contsel, JOIN = contjoinsel );
CREATE OPERATOR <<= ( LEFTARG = iprange, RIGHTARG = iprange, PROCEDURE = iprange_contained_by, COMMUTATOR = '>>=', RESTRICT = contsel, JOIN = contjoinsel );

CREATE FUNCTION ip4r_contained_by_strict(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_contained_by_strict(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_contained_by_strict(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR << ( LEFTARG = ip4r,    RIGHTARG = ip4r,    PROCEDURE = ip4r_contained_by_strict,    COMMUTATOR = '>>', RESTRICT = contsel, JOIN = contjoinsel );
CREATE OPERATOR << ( LEFTARG = ip6r,    RIGHTARG = ip6r,    PROCEDURE = ip6r_contained_by_strict,    COMMUTATOR = '>>', RESTRICT = contsel, JOIN = contjoinsel );
CREATE OPERATOR << ( LEFTARG = iprange, RIGHTARG = iprange, PROCEDURE = iprange_contained_by_strict, COMMUTATOR = '>>', RESTRICT = contsel, JOIN = contjoinsel );

CREATE FUNCTION ip4r_contains(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_contains(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_contains(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR >>= ( LEFTARG = ip4r,    RIGHTARG = ip4r,    PROCEDURE = ip4r_contains,    COMMUTATOR = '<<=', RESTRICT = contsel, JOIN = contjoinsel );
CREATE OPERATOR >>= ( LEFTARG = ip6r,    RIGHTARG = ip6r,    PROCEDURE = ip6r_contains,    COMMUTATOR = '<<=', RESTRICT = contsel, JOIN = contjoinsel );
CREATE OPERATOR >>= ( LEFTARG = iprange, RIGHTARG = iprange, PROCEDURE = iprange_contains, COMMUTATOR = '<<=', RESTRICT = contsel, JOIN = contjoinsel );

CREATE FUNCTION ip4r_contains_strict(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_contains_strict(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_contains_strict(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR >> ( LEFTARG = ip4r,    RIGHTARG = ip4r,    PROCEDURE = ip4r_contains_strict,    COMMUTATOR = '<<', RESTRICT = contsel, JOIN = contjoinsel );
CREATE OPERATOR >> ( LEFTARG = ip6r,    RIGHTARG = ip6r,    PROCEDURE = ip6r_contains_strict,    COMMUTATOR = '<<', RESTRICT = contsel, JOIN = contjoinsel );
CREATE OPERATOR >> ( LEFTARG = iprange, RIGHTARG = iprange, PROCEDURE = iprange_contains_strict, COMMUTATOR = '<<', RESTRICT = contsel, JOIN = contjoinsel );

CREATE FUNCTION ip4r_overlaps(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_overlaps(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_overlaps(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR && ( LEFTARG = ip4r,    RIGHTARG = ip4r,    PROCEDURE = ip4r_overlaps,    COMMUTATOR = '&&', RESTRICT = areasel, JOIN = areajoinsel );
CREATE OPERATOR && ( LEFTARG = ip6r,    RIGHTARG = ip6r,    PROCEDURE = ip6r_overlaps,    COMMUTATOR = '&&', RESTRICT = areasel, JOIN = areajoinsel );
CREATE OPERATOR && ( LEFTARG = iprange, RIGHTARG = iprange, PROCEDURE = iprange_overlaps, COMMUTATOR = '&&', RESTRICT = areasel, JOIN = areajoinsel );

-- cross-type containment
-- no operators for these since they seem to do more harm than good. These cases
-- are handled by implicit casts instead.

CREATE FUNCTION ip4_contained_by(ip4,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4_contained_by(ip4,iprange) RETURNS bool AS 'MODULE_PATHNAME','iprange_ip4_contained_by' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_contained_by(ip6,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_contained_by(ip6,iprange) RETURNS bool AS 'MODULE_PATHNAME','iprange_ip6_contained_by' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_contained_by(ipaddress,iprange) RETURNS bool AS 'MODULE_PATHNAME','iprange_ip_contained_by' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip4_contains(ip4r,ip4) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_contains(ip6r,ip6) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4_contains(iprange,ip4) RETURNS bool AS 'MODULE_PATHNAME','iprange_contains_ip4' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_contains(iprange,ip6) RETURNS bool AS 'MODULE_PATHNAME','iprange_contains_ip6' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_contains(iprange,ipaddress) RETURNS bool AS 'MODULE_PATHNAME','iprange_contains_ip' LANGUAGE C IMMUTABLE STRICT;

-- btree (strict weak) ordering operators
-- meaning of < > for ip4 and ip6 is obvious.
-- for ipaddress, all ip4 addresses are less than all ip6 addresses
-- for ip4r/ip6r, the order is lexicographic on (lower,upper)
-- for iprange, the universal range is lowest, then all ip4 ranges, then ip6

CREATE FUNCTION ip4_eq(ip4,ip4) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_eq(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_eq(ip6,ip6) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_eq(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_eq(ipaddress,ipaddress) RETURNS bool AS 'MODULE_PATHNAME','ipaddr_eq' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_eq(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR = ( LEFTARG = ip4,  RIGHTARG = ip4,  PROCEDURE = ip4_eq,  COMMUTATOR = '=', NEGATOR = '<>', RESTRICT = eqsel, JOIN = eqjoinsel, SORT1 = '<', SORT2 = '<', HASHES );
CREATE OPERATOR = ( LEFTARG = ip4r, RIGHTARG = ip4r, PROCEDURE = ip4r_eq, COMMUTATOR = '=', NEGATOR = '<>', RESTRICT = eqsel, JOIN = eqjoinsel, SORT1 = '<', SORT2 = '<', HASHES );
CREATE OPERATOR = ( LEFTARG = ip6,  RIGHTARG = ip6,  PROCEDURE = ip6_eq,  COMMUTATOR = '=', NEGATOR = '<>', RESTRICT = eqsel, JOIN = eqjoinsel, SORT1 = '<', SORT2 = '<', HASHES );
CREATE OPERATOR = ( LEFTARG = ip6r, RIGHTARG = ip6r, PROCEDURE = ip6r_eq, COMMUTATOR = '=', NEGATOR = '<>', RESTRICT = eqsel, JOIN = eqjoinsel, SORT1 = '<', SORT2 = '<', HASHES );
CREATE OPERATOR = ( LEFTARG = ipaddress, RIGHTARG = ipaddress, PROCEDURE = ipaddress_eq, COMMUTATOR = '=', NEGATOR = '<>', RESTRICT = eqsel, JOIN = eqjoinsel, SORT1 = '<', SORT2 = '<', HASHES );
CREATE OPERATOR = ( LEFTARG = iprange,   RIGHTARG = iprange,   PROCEDURE = iprange_eq,   COMMUTATOR = '=', NEGATOR = '<>', RESTRICT = eqsel, JOIN = eqjoinsel, SORT1 = '<', SORT2 = '<', HASHES );

CREATE FUNCTION ip4_ge(ip4,ip4) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_ge(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_ge(ip6,ip6) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_ge(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_ge(ipaddress,ipaddress) RETURNS bool AS 'MODULE_PATHNAME','ipaddr_ge' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_ge(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR >= ( LEFTARG = ip4,  RIGHTARG = ip4,  PROCEDURE = ip4_ge,  COMMUTATOR = '<=', NEGATOR = '<', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );
CREATE OPERATOR >= ( LEFTARG = ip4r, RIGHTARG = ip4r, PROCEDURE = ip4r_ge, COMMUTATOR = '<=', NEGATOR = '<', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );
CREATE OPERATOR >= ( LEFTARG = ip6,  RIGHTARG = ip6,  PROCEDURE = ip6_ge,  COMMUTATOR = '<=', NEGATOR = '<', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );
CREATE OPERATOR >= ( LEFTARG = ip6r, RIGHTARG = ip6r, PROCEDURE = ip6r_ge, COMMUTATOR = '<=', NEGATOR = '<', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );
CREATE OPERATOR >= ( LEFTARG = ipaddress, RIGHTARG = ipaddress, PROCEDURE = ipaddress_ge, COMMUTATOR = '<=', NEGATOR = '<', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );
CREATE OPERATOR >= ( LEFTARG = iprange,   RIGHTARG = iprange,   PROCEDURE = iprange_ge,   COMMUTATOR = '<=', NEGATOR = '<', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );

CREATE FUNCTION ip4_gt(ip4,ip4) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_gt(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_gt(ip6,ip6) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_gt(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_gt(ipaddress,ipaddress) RETURNS bool AS 'MODULE_PATHNAME','ipaddr_gt' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_gt(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR > ( LEFTARG = ip4,  RIGHTARG = ip4,  PROCEDURE = ip4_gt,  COMMUTATOR = '<', NEGATOR = '<=', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );
CREATE OPERATOR > ( LEFTARG = ip4r, RIGHTARG = ip4r, PROCEDURE = ip4r_gt, COMMUTATOR = '<', NEGATOR = '<=', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );
CREATE OPERATOR > ( LEFTARG = ip6,  RIGHTARG = ip6,  PROCEDURE = ip6_gt,  COMMUTATOR = '<', NEGATOR = '<=', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );
CREATE OPERATOR > ( LEFTARG = ip6r, RIGHTARG = ip6r, PROCEDURE = ip6r_gt, COMMUTATOR = '<', NEGATOR = '<=', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );
CREATE OPERATOR > ( LEFTARG = ipaddress, RIGHTARG = ipaddress, PROCEDURE = ipaddress_gt, COMMUTATOR = '<', NEGATOR = '<=', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );
CREATE OPERATOR > ( LEFTARG = iprange,   RIGHTARG = iprange,   PROCEDURE = iprange_gt,   COMMUTATOR = '<', NEGATOR = '<=', RESTRICT = scalargtsel, JOIN = scalargtjoinsel );

CREATE FUNCTION ip4_le(ip4,ip4) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_le(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_le(ip6,ip6) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_le(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_le(ipaddress,ipaddress) RETURNS bool AS 'MODULE_PATHNAME','ipaddr_le' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_le(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR <= ( LEFTARG = ip4,  RIGHTARG = ip4,  PROCEDURE = ip4_le,  COMMUTATOR = '>=', NEGATOR = '>', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );
CREATE OPERATOR <= ( LEFTARG = ip4r, RIGHTARG = ip4r, PROCEDURE = ip4r_le, COMMUTATOR = '>=', NEGATOR = '>', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );
CREATE OPERATOR <= ( LEFTARG = ip6,  RIGHTARG = ip6,  PROCEDURE = ip6_le,  COMMUTATOR = '>=', NEGATOR = '>', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );
CREATE OPERATOR <= ( LEFTARG = ip6r, RIGHTARG = ip6r, PROCEDURE = ip6r_le, COMMUTATOR = '>=', NEGATOR = '>', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );
CREATE OPERATOR <= ( LEFTARG = ipaddress, RIGHTARG = ipaddress, PROCEDURE = ipaddress_le, COMMUTATOR = '>=', NEGATOR = '>', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );
CREATE OPERATOR <= ( LEFTARG = iprange,   RIGHTARG = iprange,   PROCEDURE = iprange_le,   COMMUTATOR = '>=', NEGATOR = '>', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );

CREATE FUNCTION ip4_lt(ip4,ip4) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_lt(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_lt(ip6,ip6) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_lt(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_lt(ipaddress,ipaddress) RETURNS bool AS 'MODULE_PATHNAME','ipaddr_lt' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_lt(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR < ( LEFTARG = ip4,  RIGHTARG = ip4,  PROCEDURE = ip4_lt,  COMMUTATOR = '>', NEGATOR = '>=', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );
CREATE OPERATOR < ( LEFTARG = ip4r, RIGHTARG = ip4r, PROCEDURE = ip4r_lt, COMMUTATOR = '>', NEGATOR = '>=', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );
CREATE OPERATOR < ( LEFTARG = ip6,  RIGHTARG = ip6,  PROCEDURE = ip6_lt,  COMMUTATOR = '>', NEGATOR = '>=', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );
CREATE OPERATOR < ( LEFTARG = ip6r, RIGHTARG = ip6r, PROCEDURE = ip6r_lt, COMMUTATOR = '>', NEGATOR = '>=', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );
CREATE OPERATOR < ( LEFTARG = ipaddress, RIGHTARG = ipaddress, PROCEDURE = ipaddress_lt, COMMUTATOR = '>', NEGATOR = '>=', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );
CREATE OPERATOR < ( LEFTARG = iprange,   RIGHTARG = iprange,   PROCEDURE = iprange_lt,   COMMUTATOR = '>', NEGATOR = '>=', RESTRICT = scalarltsel, JOIN = scalarltjoinsel );

CREATE FUNCTION ip4_neq(ip4,ip4) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_neq(ip4r,ip4r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_neq(ip6,ip6) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_neq(ip6r,ip6r) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_neq(ipaddress,ipaddress) RETURNS bool AS 'MODULE_PATHNAME','ipaddr_neq' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_neq(iprange,iprange) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR <> ( LEFTARG = ip4,  RIGHTARG = ip4,  PROCEDURE = ip4_neq,  COMMUTATOR = '<>', NEGATOR = '=', RESTRICT = neqsel, JOIN = neqjoinsel );
CREATE OPERATOR <> ( LEFTARG = ip4r, RIGHTARG = ip4r, PROCEDURE = ip4r_neq, COMMUTATOR = '<>', NEGATOR = '=', RESTRICT = neqsel, JOIN = neqjoinsel );
CREATE OPERATOR <> ( LEFTARG = ip6,  RIGHTARG = ip6,  PROCEDURE = ip6_neq,  COMMUTATOR = '<>', NEGATOR = '=', RESTRICT = neqsel, JOIN = neqjoinsel );
CREATE OPERATOR <> ( LEFTARG = ip6r, RIGHTARG = ip6r, PROCEDURE = ip6r_neq, COMMUTATOR = '<>', NEGATOR = '=', RESTRICT = neqsel, JOIN = neqjoinsel );
CREATE OPERATOR <> ( LEFTARG = ipaddress, RIGHTARG = ipaddress, PROCEDURE = ipaddress_neq, COMMUTATOR = '<>', NEGATOR = '=', RESTRICT = neqsel, JOIN = neqjoinsel );
CREATE OPERATOR <> ( LEFTARG = iprange,   RIGHTARG = iprange,   PROCEDURE = iprange_neq,   COMMUTATOR = '<>', NEGATOR = '=', RESTRICT = neqsel, JOIN = neqjoinsel );

-- ----------------------------------------------------------------------
-- Btree index

CREATE FUNCTION ip4_cmp(ip4,ip4) RETURNS integer AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_cmp(ip4r,ip4r) RETURNS integer AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_cmp(ip6,ip6) RETURNS integer AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_cmp(ip6r,ip6r) RETURNS integer AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_cmp(ipaddress,ipaddress) RETURNS integer AS 'MODULE_PATHNAME','ipaddr_cmp' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_cmp(iprange,iprange) RETURNS integer AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION in_range(ip4,ip4,bigint,boolean,boolean) RETURNS boolean AS 'MODULE_PATHNAME','ip4_in_range_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION in_range(ip4,ip4,ip4,boolean,boolean) RETURNS boolean AS 'MODULE_PATHNAME','ip4_in_range_ip4' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION in_range(ip6,ip6,bigint,boolean,boolean) RETURNS boolean AS 'MODULE_PATHNAME','ip6_in_range_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION in_range(ip6,ip6,ip6,boolean,boolean) RETURNS boolean AS 'MODULE_PATHNAME','ip6_in_range_ip6' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR CLASS btree_ip4_ops DEFAULT FOR TYPE ip4 USING btree AS
       OPERATOR	1	< ,
       OPERATOR	2	<= ,
       OPERATOR	3	= ,
       OPERATOR	4	>= ,
       OPERATOR	5	> ,
       FUNCTION	1	ip4_cmp(ip4, ip4);

CREATE OPERATOR CLASS btree_ip4r_ops DEFAULT FOR TYPE ip4r USING btree AS
       OPERATOR	1	< ,
       OPERATOR	2	<= ,
       OPERATOR	3	= ,
       OPERATOR	4	>= ,
       OPERATOR	5	> ,
       FUNCTION	1	ip4r_cmp(ip4r, ip4r);

CREATE OPERATOR CLASS btree_ip6_ops DEFAULT FOR TYPE ip6 USING btree AS
       OPERATOR	1	< ,
       OPERATOR	2	<= ,
       OPERATOR	3	= ,
       OPERATOR	4	>= ,
       OPERATOR	5	> ,
       FUNCTION	1	ip6_cmp(ip6, ip6);

CREATE OPERATOR CLASS btree_ip6r_ops DEFAULT FOR TYPE ip6r USING btree AS
       OPERATOR	1	< ,
       OPERATOR	2	<= ,
       OPERATOR	3	= ,
       OPERATOR	4	>= ,
       OPERATOR	5	> ,
       FUNCTION	1	ip6r_cmp(ip6r, ip6r);

CREATE OPERATOR CLASS btree_ipaddress_ops DEFAULT FOR TYPE ipaddress USING btree AS
       OPERATOR	1	< ,
       OPERATOR	2	<= ,
       OPERATOR	3	= ,
       OPERATOR	4	>= ,
       OPERATOR	5	> ,
       FUNCTION	1	ipaddress_cmp(ipaddress, ipaddress);

CREATE OPERATOR CLASS btree_iprange_ops DEFAULT FOR TYPE iprange USING btree AS
       OPERATOR	1	< ,
       OPERATOR	2	<= ,
       OPERATOR	3	= ,
       OPERATOR	4	>= ,
       OPERATOR	5	> ,
       FUNCTION	1	iprange_cmp(iprange, iprange);

-- ----------------------------------------------------------------------
-- Hash index

-- the hash index definitions are needed for hashagg, hashjoin, hash-distinct, hashsetop
-- etc. even if no actual hash indexes are used.

CREATE FUNCTION ip4hash(ip4) RETURNS integer AS 'MODULE_PATHNAME', 'ip4hash' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6hash(ip6) RETURNS integer AS 'MODULE_PATHNAME', 'ip6hash' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddresshash(ipaddress) RETURNS integer AS 'MODULE_PATHNAME', 'ipaddr_hash' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4rhash(ip4r) RETURNS integer AS 'MODULE_PATHNAME', 'ip4rhash' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6rhash(ip6r) RETURNS integer AS 'MODULE_PATHNAME', 'ip6rhash' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprangehash(iprange) RETURNS integer AS 'MODULE_PATHNAME', 'iprange_hash' LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION iprangehash(iprange) IS 'deprecated, obsolete';
CREATE FUNCTION iprange_hash(iprange) RETURNS integer AS 'MODULE_PATHNAME', 'iprange_hash_new' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ip4_hash_extended(ip4,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'ip4_hash_extended' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_hash_extended(ip6,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'ip6_hash_extended' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_hash_extended(ipaddress,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'ipaddr_hash_extended' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_hash_extended(ip4r,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'ip4r_hash_extended' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_hash_extended(ip6r,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'ip6r_hash_extended' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_hash_extended(iprange,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'iprange_hash_extended' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR CLASS hash_ip4_ops DEFAULT FOR TYPE ip4 USING hash AS
       OPERATOR	1	= ,
       FUNCTION	1	ip4hash(ip4);

CREATE OPERATOR CLASS hash_ip4r_ops DEFAULT FOR TYPE ip4r USING hash AS
       OPERATOR	1	= ,
       FUNCTION	1	ip4rhash(ip4r);

CREATE OPERATOR CLASS hash_ip6_ops DEFAULT FOR TYPE ip6 USING hash AS
       OPERATOR	1	= ,
       FUNCTION	1	ip6hash(ip6);

CREATE OPERATOR CLASS hash_ip6r_ops DEFAULT FOR TYPE ip6r USING hash AS
       OPERATOR	1	= ,
       FUNCTION	1	ip6rhash(ip6r);

CREATE OPERATOR CLASS hash_ipaddress_ops DEFAULT FOR TYPE ipaddress USING hash AS
       OPERATOR	1	= ,
       FUNCTION	1	ipaddresshash(ipaddress);

CREATE OPERATOR CLASS hash_iprange_ops DEFAULT FOR TYPE iprange USING hash AS
       OPERATOR	1	= ,
       FUNCTION	1	iprange_hash(iprange);

-- ----------------------------------------------------------------------
-- GiST

CREATE FUNCTION gip4r_consistent(internal,ip4r,int2,oid,internal) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip4r_compress(internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip4r_decompress(internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip4r_penalty(internal,internal,internal) RETURNS internal AS 'MODULE_PATHNAME' LANGUAGE C STRICT;
CREATE FUNCTION gip4r_picksplit(internal, internal) RETURNS internal AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip4r_union(internal, internal) RETURNS ip4r AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip4r_same(ip4r, ip4r, internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip4r_fetch(internal) RETURNS internal	AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION gip6r_consistent(internal,ip6r,int2,oid,internal) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip6r_compress(internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip6r_decompress(internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip6r_penalty(internal,internal,internal) RETURNS internal AS 'MODULE_PATHNAME' LANGUAGE C STRICT;
CREATE FUNCTION gip6r_picksplit(internal, internal) RETURNS internal AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip6r_union(internal, internal) RETURNS ip6r AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip6r_same(ip6r, ip6r, internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip6r_fetch(internal) RETURNS internal	AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE FUNCTION gipr_consistent(internal,iprange,int2,oid,internal) RETURNS bool AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gipr_compress(internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gipr_decompress(internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gipr_penalty(internal,internal,internal) RETURNS internal AS 'MODULE_PATHNAME' LANGUAGE C STRICT;
CREATE FUNCTION gipr_picksplit(internal, internal) RETURNS internal AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gipr_union(internal, internal) RETURNS iprange AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gipr_same(iprange, iprange, internal) RETURNS internal	AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gipr_fetch(internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;

CREATE OPERATOR CLASS gist_ip4r_ops DEFAULT FOR TYPE ip4r USING gist AS
       OPERATOR	1	>>= ,
       OPERATOR	2	<<= ,
       OPERATOR	3	>> ,
       OPERATOR	4	<< ,
       OPERATOR	5	&& ,
       OPERATOR	6	= ,
       FUNCTION	1	gip4r_consistent (internal, ip4r, int2, oid, internal),
       FUNCTION	2	gip4r_union (internal, internal),
       FUNCTION	3	gip4r_compress (internal),
       FUNCTION	4	gip4r_decompress (internal),
       FUNCTION	5	gip4r_penalty (internal, internal, internal),
       FUNCTION	6	gip4r_picksplit (internal, internal),
       FUNCTION	7	gip4r_same (ip4r, ip4r, internal);

CREATE OPERATOR CLASS gist_ip6r_ops DEFAULT FOR TYPE ip6r USING gist AS
       OPERATOR	1	>>= ,
       OPERATOR	2	<<= ,
       OPERATOR	3	>> ,
       OPERATOR	4	<< ,
       OPERATOR	5	&& ,
       OPERATOR	6	= ,
       FUNCTION	1	gip6r_consistent (internal, ip6r, int2, oid, internal),
       FUNCTION	2	gip6r_union (internal, internal),
       FUNCTION	3	gip6r_compress (internal),
       FUNCTION	4	gip6r_decompress (internal),
       FUNCTION	5	gip6r_penalty (internal, internal, internal),
       FUNCTION	6	gip6r_picksplit (internal, internal),
       FUNCTION	7	gip6r_same (ip6r, ip6r, internal);

CREATE OPERATOR CLASS gist_iprange_ops DEFAULT FOR TYPE iprange USING gist AS
       OPERATOR	1	>>= ,
       OPERATOR	2	<<= ,
       OPERATOR	3	>> ,
       OPERATOR	4	<< ,
       OPERATOR	5	&& ,
       OPERATOR	6	= ,
       FUNCTION	1	gipr_consistent (internal, iprange, int2, oid, internal),
       FUNCTION	2	gipr_union (internal, internal),
       FUNCTION	3	gipr_compress (internal),
       FUNCTION	4	gipr_decompress (internal),
       FUNCTION	5	gipr_penalty (internal, internal, internal),
       FUNCTION	6	gipr_picksplit (internal, internal),
       FUNCTION	7	gipr_same (iprange, iprange, internal);

DO $s$
  DECLARE
    pg_ver integer := current_setting('server_version_num')::integer;
    r record;
  BEGIN
    IF pg_ver >= 90500 THEN
      ALTER OPERATOR FAMILY gist_ip4r_ops USING gist ADD
	     FUNCTION	9  (ip4r,ip4r)	gip4r_fetch (internal);
      ALTER OPERATOR FAMILY gist_ip6r_ops USING gist ADD
	     FUNCTION	9  (ip6r,ip6r)	gip6r_fetch (internal);
      ALTER OPERATOR FAMILY gist_iprange_ops USING gist ADD
	     FUNCTION	9  (iprange,iprange)	gipr_fetch (internal);
    END IF;
    IF pg_ver >= 90600 THEN
      FOR r IN SELECT oid::regprocedure as fsig
		 FROM pg_catalog.pg_proc
		WHERE (probin = 'MODULE_PATHNAME'
		       AND prolang = (SELECT oid FROM pg_catalog.pg_language l WHERE l.lanname='c'))
		   OR (oid in ('family(ip4)'::regprocedure,
			       'family(ip6)'::regprocedure,
			       'family(ip4r)'::regprocedure,
			       'family(ip6r)'::regprocedure))
      LOOP
	EXECUTE format('ALTER FUNCTION %s PARALLEL SAFE', r.fsig);
      END LOOP;
    END IF;
    IF pg_ver >= 110000 THEN
      FOR r IN SELECT tname
		 FROM UNNEST(ARRAY['ip4','ip4r',
				   'ip6','ip6r',
				   'ipaddress','iprange']) u(tname)
      LOOP
	EXECUTE format('ALTER OPERATOR FAMILY %I USING hash'
		       '  ADD FUNCTION 2 %I(%I,bigint)',
		       format('hash_%s_ops', r.tname),
		       format('%s_hash_extended', r.tname),
		       r.tname);
      END LOOP;
      ALTER OPERATOR FAMILY btree_ip4_ops USING btree
        ADD FUNCTION 3 (ip4,bigint) in_range(ip4,ip4,bigint,boolean,boolean);
      ALTER OPERATOR FAMILY btree_ip4_ops USING btree
        ADD FUNCTION 3 (ip4,ip4) in_range(ip4,ip4,ip4,boolean,boolean);
      ALTER OPERATOR FAMILY btree_ip6_ops USING btree
        ADD FUNCTION 3 (ip6,bigint) in_range(ip6,ip6,bigint,boolean,boolean);
      ALTER OPERATOR FAMILY btree_ip6_ops USING btree
        ADD FUNCTION 3 (ip6,ip6) in_range(ip6,ip6,ip6,boolean,boolean);
    END IF;
  END;
$s$;

-- end
