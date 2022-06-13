/* ip4r--2.2--2.4.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION ip4r UPDATE TO '2.4'" to load this file. \quit

-- new funcs
CREATE FUNCTION cidr_split(ip4r) RETURNS SETOF ip4r AS 'MODULE_PATHNAME','ip4r_cidr_split' LANGUAGE C IMMUTABLE STRICT ROWS 10;
CREATE FUNCTION cidr_split(ip6r) RETURNS SETOF ip6r AS 'MODULE_PATHNAME','ip6r_cidr_split' LANGUAGE C IMMUTABLE STRICT ROWS 50;
CREATE FUNCTION cidr_split(iprange) RETURNS SETOF iprange AS 'MODULE_PATHNAME','iprange_cidr_split' LANGUAGE C IMMUTABLE STRICT ROWS 30;

-- new casts
CREATE FUNCTION ip4(bit) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4(varbit) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4(bytea) RETURNS ip4 AS 'MODULE_PATHNAME','ip4_cast_from_bytea' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6(bit) RETURNS ip6 AS 'MODULE_PATHNAME','ip6_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6(varbit) RETURNS ip6 AS 'MODULE_PATHNAME','ip6_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6(bytea) RETURNS ip6 AS 'MODULE_PATHNAME','ip6_cast_from_bytea' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress(bit) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress(varbit) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress(bytea) RETURNS ipaddress AS 'MODULE_PATHNAME','ipaddr_cast_from_bytea' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r(varbit) RETURNS ip4r AS 'MODULE_PATHNAME','ip4r_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r(varbit) RETURNS ip6r AS 'MODULE_PATHNAME','ip6r_cast_from_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(ip4) RETURNS varbit AS 'MODULE_PATHNAME','ip4_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(ip6) RETURNS varbit AS 'MODULE_PATHNAME','ip6_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(ipaddress) RETURNS varbit AS 'MODULE_PATHNAME','ipaddr_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(ip4r) RETURNS varbit AS 'MODULE_PATHNAME','ip4r_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(ip6r) RETURNS varbit AS 'MODULE_PATHNAME','ip6r_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bit(iprange) RETURNS varbit AS 'MODULE_PATHNAME','iprange_cast_to_bit' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bytea(ip4) RETURNS bytea AS 'MODULE_PATHNAME','ip4_cast_to_bytea' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bytea(ip6) RETURNS bytea AS 'MODULE_PATHNAME','ip6_cast_to_bytea' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION to_bytea(ipaddress) RETURNS bytea AS 'MODULE_PATHNAME','ipaddr_cast_to_bytea' LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (ip4 as varbit) WITH FUNCTION to_bit(ip4);
CREATE CAST (ip4 as bytea) WITH FUNCTION to_bytea(ip4);
CREATE CAST (ip4r as varbit) WITH FUNCTION to_bit(ip4r);
CREATE CAST (ip6 as varbit) WITH FUNCTION to_bit(ip6);
CREATE CAST (ip6 as bytea) WITH FUNCTION to_bytea(ip6);
CREATE CAST (ip6r as varbit) WITH FUNCTION to_bit(ip6r);
CREATE CAST (ipaddress as varbit) WITH FUNCTION to_bit(ipaddress);
CREATE CAST (ipaddress as bytea) WITH FUNCTION to_bytea(ipaddress);
CREATE CAST (iprange as varbit) WITH FUNCTION to_bit(iprange);
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

-- new hash funcs
COMMENT ON FUNCTION iprangehash(iprange) IS 'deprecated, obsolete';
CREATE FUNCTION iprange_hash(iprange) RETURNS integer AS 'MODULE_PATHNAME', 'iprange_hash_new' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4_hash_extended(ip4,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'ip4_hash_extended' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6_hash_extended(ip6,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'ip6_hash_extended' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ipaddress_hash_extended(ipaddress,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'ipaddr_hash_extended' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip4r_hash_extended(ip4r,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'ip4r_hash_extended' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ip6r_hash_extended(ip6r,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'ip6r_hash_extended' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION iprange_hash_extended(iprange,bigint) RETURNS bigint AS 'MODULE_PATHNAME', 'iprange_hash_extended' LANGUAGE C IMMUTABLE STRICT;

-- new btree/range funcs
CREATE FUNCTION in_range(ip4,ip4,bigint,boolean,boolean) RETURNS boolean AS 'MODULE_PATHNAME','ip4_in_range_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION in_range(ip4,ip4,ip4,boolean,boolean) RETURNS boolean AS 'MODULE_PATHNAME','ip4_in_range_ip4' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION in_range(ip6,ip6,bigint,boolean,boolean) RETURNS boolean AS 'MODULE_PATHNAME','ip6_in_range_bigint' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION in_range(ip6,ip6,ip6,boolean,boolean) RETURNS boolean AS 'MODULE_PATHNAME','ip6_in_range_ip6' LANGUAGE C IMMUTABLE STRICT;

-- drop/create hash opclass

DO $s$
  DECLARE
    r record;
    opc_oid oid;
    deps oid[];
    cmds text[] := NULL;
    str text;
    cfgval text := NULL;
  BEGIN
    BEGIN
      cfgval := current_setting('ip4r.update_indexes');
    EXCEPTION WHEN OTHERS THEN
      cfgval := NULL;
    END;
    IF cfgval NOT IN ('drop','rebuild') THEN
      cfgval := NULL;
    END IF;
    SELECT oid INTO opc_oid
      FROM pg_opclass
     WHERE opcname='hash_iprange_ops'
       AND pg_opclass_is_visible(oid);
    -- first check for unknown/unexpected dependencies
    SELECT format('%s object %s depends on hash_iprange_ops with type ''%s''',
                  classid::regclass, objid, deptype)
      INTO str
      FROM pg_depend
     WHERE refclassid = 'pg_opclass'::regclass
       AND refobjid = opc_oid
       AND classid <> 'pg_class'::regclass
       AND deptype NOT IN ('a','i')
     LIMIT 1;
    IF FOUND THEN
      RAISE EXCEPTION dependent_objects_still_exist USING
        MESSAGE = 'unexpected dependency on hash opclass',
	DETAIL = str;
    END IF;
    -- find all dependent tables and indexes
    deps := ARRAY(SELECT objid FROM pg_depend
                   WHERE refclassid = 'pg_opclass'::regclass
		     AND refobjid = opc_oid
		     AND classid = 'pg_class'::regclass
		     AND deptype = 'n');
    IF deps <> '{}' THEN
      -- we don't expect to find anything except indexes
      SELECT format('Table %s depends on hash_iprange_ops', oid::regclass)
        INTO str
        FROM pg_class
       WHERE oid = ANY (deps)
         AND relkind <> 'i';
      IF FOUND THEN
        RAISE EXCEPTION dependent_objects_still_exist USING
          MESSAGE = 'unexpected table dependency on hash opclass',
	  DETAIL = str;
      END IF;
      -- must be only indexes, as expected
      IF cfgval IS NULL THEN
        FOR r IN SELECT ci.relname as indexname,
	                n.nspname as schemaname,
			ct.relname as tablename
	           FROM pg_index i
		   JOIN pg_class ci ON ci.oid=i.indexrelid
		   JOIN pg_class ct ON ct.oid=i.indrelid
		   JOIN pg_namespace n ON n.oid=ct.relnamespace
		   WHERE i.indexrelid = ANY (deps)
        LOOP
	  RAISE INFO USING
	    MESSAGE = format('index %I on table %I.%I depends on hash_iprange_ops',
	                     r.indexname, r.schemaname, r.tablename);
	END LOOP;
        RAISE EXCEPTION dependent_objects_still_exist USING
          MESSAGE = 'existing indexes depend on hash opclass',
	  DETAIL = 'See previous INFO messages for list',
	  HINT = 'Use SET ip4r.update_indexes = ''drop'' or ''rebuild'' to process automatically';
      ELSIF cfgval = 'drop' THEN
        CREATE SCHEMA ip4r_update_to_2_4;
	ALTER EXTENSION ip4r DROP SCHEMA ip4r_update_to_2_4;
	CREATE TABLE ip4r_update_to_2_4.update_indexes
	  AS SELECT ct.oid as table_oid,
	            n.nspname as schemaname,
		    ct.relname as tablename,
		    ci.relname as indexname,
		    pg_get_indexdef(ci.oid) as command
	       FROM pg_index i
		    JOIN pg_class ci ON ci.oid=i.indexrelid
		    JOIN pg_class ct ON ct.oid=i.indrelid
		    JOIN pg_namespace n ON n.oid=ct.relnamespace
		    WHERE i.indexrelid = ANY (deps);
        ALTER EXTENSION ip4r DROP TABLE ip4r_update_to_2_4.update_indexes;
	FOR r IN SELECT format('DROP INDEX %s RESTRICT', o::regclass) as cmd
	           FROM UNNEST(deps) u(o) LOOP
          RAISE INFO 'executing %', r.cmd;
	  EXECUTE r.cmd;
	END LOOP;
      ELSIF cfgval = 'rebuild' THEN
        cmds := '{}';
	FOR r IN SELECT format('DROP INDEX %s RESTRICT', o::regclass) as dropcmd,
	                pg_get_indexdef(o) as createcmd
	           FROM UNNEST(deps) u(o) LOOP
          RAISE INFO 'executing %', r.dropcmd;
	  EXECUTE r.dropcmd;
	  cmds := cmds || r.createcmd;
	END LOOP;
      END IF;
    END IF;
    -- have now processed any dependencies, so try the actual drop
    -- this will error out on the RESTRICT if we somehow missed any
    -- relevant dependency in our checks.
    ALTER EXTENSION ip4r DROP OPERATOR CLASS hash_iprange_ops USING hash;
    DROP OPERATOR CLASS hash_iprange_ops USING hash RESTRICT;
    CREATE OPERATOR CLASS hash_iprange_ops DEFAULT FOR TYPE iprange USING hash AS
           OPERATOR 1  = ,
           FUNCTION 1  iprange_hash(iprange);
    IF cmds IS NOT NULL THEN
      FOR r IN SELECT cmd FROM UNNEST(cmds) u(cmd) LOOP
        -- we rely here on CREATE INDEX not recording a dependency on
	-- the extension
        RAISE INFO 'executing %', r.cmd;
        EXECUTE r.cmd;
      END LOOP;
      RAISE INFO 'index rebuilds completed';
    ELSIF cfgval = 'drop' AND deps <> '{}' THEN
      RAISE LOG 'table ip4r_update_to_2_4.update_indexes was created';
      RAISE INFO 'table ip4r_update_to_2_4.update_indexes was created';
    END IF;
  END;
$s$;

DO $s$
  DECLARE
    pg_ver integer := current_setting('server_version_num')::integer;
    r record;
  BEGIN
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
