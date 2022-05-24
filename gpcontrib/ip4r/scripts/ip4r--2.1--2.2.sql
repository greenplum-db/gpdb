/* ip4r--2.1--2.2.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION ip4r UPDATE TO '2.2'" to load this file. \quit

-- There's nothing actually new in this version, but it fixes an
-- incorrect fix in 2.1.

WITH v(gname,gtype) AS (
  VALUES ('gip4r_consistent'::name, 'ip4r'::regtype),
         ('gip6r_consistent'::name, 'ip6r'::regtype),
         ('gipr_consistent'::name,  'iprange'::regtype))
UPDATE pg_catalog.pg_proc
   SET pronargs = 5,
       proargtypes = array_to_string(array['internal',
                                           v.gtype,
                                           'int2',
                                           'oid',
                                           'internal']::regtype[]::oid[],
                                     ' ')::pg_catalog.oidvector
  FROM v
 WHERE proname = v.gname
   AND probin = 'MODULE_PATHNAME';

-- end
