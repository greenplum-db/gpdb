/* ip4r--2.0--2.1.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION ip4r UPDATE TO '2.1'" to load this file. \quit

-- ugh. no ALTER CAST

UPDATE pg_catalog.pg_cast SET castcontext = 'a'
 WHERE (castsource,casttarget) IN (
        ('ipaddress'::regtype, 'ip4'::regtype),
        ('ipaddress'::regtype, 'ip6'::regtype),
        ('iprange'::regtype, 'ip4r'::regtype),
        ('iprange'::regtype, 'ip6r'::regtype));

-- double ugh, to finally fix long-standing issue with function signature
-- of gist consistent functions

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

-- actual new stuff

CREATE FUNCTION gip4r_fetch(internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gip6r_fetch(internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;
CREATE FUNCTION gipr_fetch(internal) RETURNS internal  AS 'MODULE_PATHNAME' LANGUAGE C;

DO $s$
  BEGIN
    IF current_setting('server_version_num')::integer >= 90500 THEN
      ALTER OPERATOR FAMILY gist_ip4r_ops USING gist ADD
             FUNCTION	9  (ip4r,ip4r)	gip4r_fetch (internal);
      ALTER OPERATOR FAMILY gist_ip6r_ops USING gist ADD
             FUNCTION	9  (ip6r,ip6r)	gip6r_fetch (internal);
      ALTER OPERATOR FAMILY gist_iprange_ops USING gist ADD
             FUNCTION	9  (iprange,iprange)	gipr_fetch (internal);
    END IF;
    IF current_setting('server_version_num')::integer >= 90600 THEN
      DECLARE
        r record;
      BEGIN
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
      END;
    END IF;
  END;
$s$;

-- end
