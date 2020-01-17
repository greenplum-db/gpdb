/*
 * PostgreSQL System Views
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Copyright (c) 1996-2016, PostgreSQL Global Development Group
 *
 * src/backend/catalog/system_views.sql
 *
 * Note: this file is read in single-user -j mode, which means that the
 * command terminator is semicolon-newline-newline; whenever the backend
 * sees that, it stops and executes what it's got.  If you write a lot of
 * statements without empty lines between, they'll all get quoted to you
 * in any error message about one of them, so don't do that.  Also, you
 * cannot write a semicolon immediately followed by an empty line in a
 * string literal (including a function body!) or a multiline comment.
 */

CREATE VIEW pg_roles AS
    SELECT
        rolname,
        rolsuper,
        rolinherit,
        rolcreaterole,
        rolcreatedb,
        rolcanlogin,
        rolreplication,
        rolconnlimit,
        '********'::text as rolpassword,
        rolvaliduntil,
        rolbypassrls,
        setconfig as rolconfig,
		rolresqueue,
        pg_authid.oid,
        rolcreaterextgpfd,
        rolcreaterexthttp,
        rolcreatewextgpfd,
        rolresgroup
    FROM pg_authid LEFT JOIN pg_db_role_setting s
    ON (pg_authid.oid = setrole AND setdatabase = 0);

CREATE VIEW pg_shadow AS
    SELECT
        rolname AS usename,
        pg_authid.oid AS usesysid,
        rolcreatedb AS usecreatedb,
        rolsuper AS usesuper,
        rolreplication AS userepl,
        rolbypassrls AS usebypassrls,
        rolpassword AS passwd,
        rolvaliduntil::abstime AS valuntil,
        setconfig AS useconfig
    FROM pg_authid LEFT JOIN pg_db_role_setting s
    ON (pg_authid.oid = setrole AND setdatabase = 0)
    WHERE rolcanlogin;

REVOKE ALL on pg_shadow FROM public;

CREATE VIEW pg_group AS
    SELECT
        rolname AS groname,
        oid AS grosysid,
        ARRAY(SELECT member FROM pg_auth_members WHERE roleid = oid) AS grolist
    FROM pg_authid
    WHERE NOT rolcanlogin;

CREATE VIEW pg_user AS
    SELECT
        usename,
        usesysid,
        usecreatedb,
        usesuper,
        userepl,
        usebypassrls,
        '********'::text as passwd,
        valuntil,
        useconfig
    FROM pg_shadow;

CREATE VIEW pg_policies AS
    SELECT
        N.nspname AS schemaname,
        C.relname AS tablename,
        pol.polname AS policyname,
        CASE
            WHEN pol.polroles = '{0}' THEN
                string_to_array('public', '')
            ELSE
                ARRAY
                (
                    SELECT rolname
                    FROM pg_catalog.pg_authid
                    WHERE oid = ANY (pol.polroles) ORDER BY 1
                )
        END AS roles,
        CASE pol.polcmd
            WHEN 'r' THEN 'SELECT'
            WHEN 'a' THEN 'INSERT'
            WHEN 'w' THEN 'UPDATE'
            WHEN 'd' THEN 'DELETE'
            WHEN '*' THEN 'ALL'
        END AS cmd,
        pg_catalog.pg_get_expr(pol.polqual, pol.polrelid) AS qual,
        pg_catalog.pg_get_expr(pol.polwithcheck, pol.polrelid) AS with_check
    FROM pg_catalog.pg_policy pol
    JOIN pg_catalog.pg_class C ON (C.oid = pol.polrelid)
    LEFT JOIN pg_catalog.pg_namespace N ON (N.oid = C.relnamespace);

CREATE VIEW pg_rules AS
    SELECT
        N.nspname AS schemaname,
        C.relname AS tablename,
        R.rulename AS rulename,
        pg_get_ruledef(R.oid) AS definition
    FROM (pg_rewrite R JOIN pg_class C ON (C.oid = R.ev_class))
        LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
    WHERE R.rulename != '_RETURN';

CREATE VIEW pg_views AS
    SELECT
        N.nspname AS schemaname,
        C.relname AS viewname,
        pg_get_userbyid(C.relowner) AS viewowner,
        pg_get_viewdef(C.oid) AS definition
    FROM pg_class C LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
    WHERE C.relkind = 'v';

CREATE VIEW pg_tables AS
    SELECT
        N.nspname AS schemaname,
        C.relname AS tablename,
        pg_get_userbyid(C.relowner) AS tableowner,
        T.spcname AS tablespace,
        C.relhasindex AS hasindexes,
        C.relhasrules AS hasrules,
        C.relhastriggers AS hastriggers,
        C.relrowsecurity AS rowsecurity
    FROM pg_class C LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
         LEFT JOIN pg_tablespace T ON (T.oid = C.reltablespace)
    WHERE C.relkind = 'r';

CREATE VIEW pg_matviews AS
    SELECT
        N.nspname AS schemaname,
        C.relname AS matviewname,
        pg_get_userbyid(C.relowner) AS matviewowner,
        T.spcname AS tablespace,
        C.relhasindex AS hasindexes,
        C.relispopulated AS ispopulated,
        pg_get_viewdef(C.oid) AS definition
    FROM pg_class C LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
         LEFT JOIN pg_tablespace T ON (T.oid = C.reltablespace)
    WHERE C.relkind = 'm';

CREATE VIEW pg_indexes AS
    SELECT
        N.nspname AS schemaname,
        C.relname AS tablename,
        I.relname AS indexname,
        T.spcname AS tablespace,
        pg_get_indexdef(I.oid) AS indexdef
    FROM pg_index X JOIN pg_class C ON (C.oid = X.indrelid)
         JOIN pg_class I ON (I.oid = X.indexrelid)
         LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
         LEFT JOIN pg_tablespace T ON (T.oid = I.reltablespace)
    WHERE C.relkind IN ('r', 'm') AND I.relkind = 'i';

CREATE VIEW pg_stats WITH (security_barrier) AS
    SELECT
        nspname AS schemaname,
        relname AS tablename,
        attname AS attname,
        stainherit AS inherited,
        stanullfrac AS null_frac,
        stawidth AS avg_width,
        stadistinct AS n_distinct,
        CASE
            WHEN stakind1 = 1 THEN stavalues1
            WHEN stakind2 = 1 THEN stavalues2
            WHEN stakind3 = 1 THEN stavalues3
            WHEN stakind4 = 1 THEN stavalues4
            WHEN stakind5 = 1 THEN stavalues5
        END AS most_common_vals,
        CASE
            WHEN stakind1 = 1 THEN stanumbers1
            WHEN stakind2 = 1 THEN stanumbers2
            WHEN stakind3 = 1 THEN stanumbers3
            WHEN stakind4 = 1 THEN stanumbers4
            WHEN stakind5 = 1 THEN stanumbers5
        END AS most_common_freqs,
        CASE
            WHEN stakind1 = 2 THEN stavalues1
            WHEN stakind2 = 2 THEN stavalues2
            WHEN stakind3 = 2 THEN stavalues3
            WHEN stakind4 = 2 THEN stavalues4
            WHEN stakind5 = 2 THEN stavalues5
        END AS histogram_bounds,
        CASE
            WHEN stakind1 = 3 THEN stanumbers1[1]
            WHEN stakind2 = 3 THEN stanumbers2[1]
            WHEN stakind3 = 3 THEN stanumbers3[1]
            WHEN stakind4 = 3 THEN stanumbers4[1]
            WHEN stakind5 = 3 THEN stanumbers5[1]
        END AS correlation,
        CASE
            WHEN stakind1 = 4 THEN stavalues1
            WHEN stakind2 = 4 THEN stavalues2
            WHEN stakind3 = 4 THEN stavalues3
            WHEN stakind4 = 4 THEN stavalues4
            WHEN stakind5 = 4 THEN stavalues5
        END AS most_common_elems,
        CASE
            WHEN stakind1 = 4 THEN stanumbers1
            WHEN stakind2 = 4 THEN stanumbers2
            WHEN stakind3 = 4 THEN stanumbers3
            WHEN stakind4 = 4 THEN stanumbers4
            WHEN stakind5 = 4 THEN stanumbers5
        END AS most_common_elem_freqs,
        CASE
            WHEN stakind1 = 5 THEN stanumbers1
            WHEN stakind2 = 5 THEN stanumbers2
            WHEN stakind3 = 5 THEN stanumbers3
            WHEN stakind4 = 5 THEN stanumbers4
            WHEN stakind5 = 5 THEN stanumbers5
        END AS elem_count_histogram
    FROM pg_statistic s JOIN pg_class c ON (c.oid = s.starelid)
         JOIN pg_attribute a ON (c.oid = attrelid AND attnum = s.staattnum)
         LEFT JOIN pg_namespace n ON (n.oid = c.relnamespace)
    WHERE NOT attisdropped
    AND has_column_privilege(c.oid, a.attnum, 'select')
    AND (c.relrowsecurity = false OR NOT row_security_active(c.oid));

REVOKE ALL on pg_statistic FROM public;

CREATE VIEW pg_locks AS
    SELECT * FROM pg_lock_status() AS L;

CREATE VIEW pg_cursors AS
    SELECT * FROM pg_cursor() AS C;

CREATE VIEW pg_available_extensions AS
    SELECT E.name, E.default_version, X.extversion AS installed_version,
           E.comment
      FROM pg_available_extensions() AS E
           LEFT JOIN pg_extension AS X ON E.name = X.extname;

CREATE VIEW pg_available_extension_versions AS
    SELECT E.name, E.version, (X.extname IS NOT NULL) AS installed,
           E.superuser, E.relocatable, E.schema, E.requires, E.comment
      FROM pg_available_extension_versions() AS E
           LEFT JOIN pg_extension AS X
             ON E.name = X.extname AND E.version = X.extversion;

CREATE VIEW pg_prepared_xacts AS
    SELECT P.transaction, P.gid, P.prepared,
           U.rolname AS owner, D.datname AS database
    FROM pg_prepared_xact() AS P
         LEFT JOIN pg_authid U ON P.ownerid = U.oid
         LEFT JOIN pg_database D ON P.dbid = D.oid;

CREATE VIEW pg_prepared_statements AS
    SELECT * FROM pg_prepared_statement() AS P;

CREATE VIEW pg_seclabels AS
SELECT
	l.objoid, l.classoid, l.objsubid,
	CASE WHEN rel.relkind = 'r' THEN 'table'::text
		 WHEN rel.relkind = 'v' THEN 'view'::text
		 WHEN rel.relkind = 'm' THEN 'materialized view'::text
		 WHEN rel.relkind = 'S' THEN 'sequence'::text
		 WHEN rel.relkind = 'f' THEN 'foreign table'::text END AS objtype,
	rel.relnamespace AS objnamespace,
	CASE WHEN pg_table_is_visible(rel.oid)
	     THEN quote_ident(rel.relname)
	     ELSE quote_ident(nsp.nspname) || '.' || quote_ident(rel.relname)
	     END AS objname,
	l.provider, l.label
FROM
	pg_seclabel l
	JOIN pg_class rel ON l.classoid = rel.tableoid AND l.objoid = rel.oid
	JOIN pg_namespace nsp ON rel.relnamespace = nsp.oid
WHERE
	l.objsubid = 0
UNION ALL
SELECT
	l.objoid, l.classoid, l.objsubid,
	'column'::text AS objtype,
	rel.relnamespace AS objnamespace,
	CASE WHEN pg_table_is_visible(rel.oid)
	     THEN quote_ident(rel.relname)
	     ELSE quote_ident(nsp.nspname) || '.' || quote_ident(rel.relname)
	     END || '.' || att.attname AS objname,
	l.provider, l.label
FROM
	pg_seclabel l
	JOIN pg_class rel ON l.classoid = rel.tableoid AND l.objoid = rel.oid
	JOIN pg_attribute att
	     ON rel.oid = att.attrelid AND l.objsubid = att.attnum
	JOIN pg_namespace nsp ON rel.relnamespace = nsp.oid
WHERE
	l.objsubid != 0
UNION ALL
SELECT
	l.objoid, l.classoid, l.objsubid,
	CASE WHEN pro.proisagg = true THEN 'aggregate'::text
	     WHEN pro.proisagg = false THEN 'function'::text
	END AS objtype,
	pro.pronamespace AS objnamespace,
	CASE WHEN pg_function_is_visible(pro.oid)
	     THEN quote_ident(pro.proname)
	     ELSE quote_ident(nsp.nspname) || '.' || quote_ident(pro.proname)
	END || '(' || pg_catalog.pg_get_function_arguments(pro.oid) || ')' AS objname,
	l.provider, l.label
FROM
	pg_seclabel l
	JOIN pg_proc pro ON l.classoid = pro.tableoid AND l.objoid = pro.oid
	JOIN pg_namespace nsp ON pro.pronamespace = nsp.oid
WHERE
	l.objsubid = 0
UNION ALL
SELECT
	l.objoid, l.classoid, l.objsubid,
	CASE WHEN typ.typtype = 'd' THEN 'domain'::text
	ELSE 'type'::text END AS objtype,
	typ.typnamespace AS objnamespace,
	CASE WHEN pg_type_is_visible(typ.oid)
	THEN quote_ident(typ.typname)
	ELSE quote_ident(nsp.nspname) || '.' || quote_ident(typ.typname)
	END AS objname,
	l.provider, l.label
FROM
	pg_seclabel l
	JOIN pg_type typ ON l.classoid = typ.tableoid AND l.objoid = typ.oid
	JOIN pg_namespace nsp ON typ.typnamespace = nsp.oid
WHERE
	l.objsubid = 0
UNION ALL
SELECT
	l.objoid, l.classoid, l.objsubid,
	'large object'::text AS objtype,
	NULL::oid AS objnamespace,
	l.objoid::text AS objname,
	l.provider, l.label
FROM
	pg_seclabel l
	JOIN pg_largeobject_metadata lom ON l.objoid = lom.oid
WHERE
	l.classoid = 'pg_catalog.pg_largeobject'::regclass AND l.objsubid = 0
UNION ALL
SELECT
	l.objoid, l.classoid, l.objsubid,
	'language'::text AS objtype,
	NULL::oid AS objnamespace,
	quote_ident(lan.lanname) AS objname,
	l.provider, l.label
FROM
	pg_seclabel l
	JOIN pg_language lan ON l.classoid = lan.tableoid AND l.objoid = lan.oid
WHERE
	l.objsubid = 0
UNION ALL
SELECT
	l.objoid, l.classoid, l.objsubid,
	'schema'::text AS objtype,
	nsp.oid AS objnamespace,
	quote_ident(nsp.nspname) AS objname,
	l.provider, l.label
FROM
	pg_seclabel l
	JOIN pg_namespace nsp ON l.classoid = nsp.tableoid AND l.objoid = nsp.oid
WHERE
	l.objsubid = 0
UNION ALL
SELECT
	l.objoid, l.classoid, l.objsubid,
	'event trigger'::text AS objtype,
	NULL::oid AS objnamespace,
	quote_ident(evt.evtname) AS objname,
	l.provider, l.label
FROM
	pg_seclabel l
	JOIN pg_event_trigger evt ON l.classoid = evt.tableoid
		AND l.objoid = evt.oid
WHERE
	l.objsubid = 0
UNION ALL
SELECT
	l.objoid, l.classoid, 0::int4 AS objsubid,
	'database'::text AS objtype,
	NULL::oid AS objnamespace,
	quote_ident(dat.datname) AS objname,
	l.provider, l.label
FROM
	pg_shseclabel l
	JOIN pg_database dat ON l.classoid = dat.tableoid AND l.objoid = dat.oid
UNION ALL
SELECT
	l.objoid, l.classoid, 0::int4 AS objsubid,
	'tablespace'::text AS objtype,
	NULL::oid AS objnamespace,
	quote_ident(spc.spcname) AS objname,
	l.provider, l.label
FROM
	pg_shseclabel l
	JOIN pg_tablespace spc ON l.classoid = spc.tableoid AND l.objoid = spc.oid
UNION ALL
SELECT
	l.objoid, l.classoid, 0::int4 AS objsubid,
	'role'::text AS objtype,
	NULL::oid AS objnamespace,
	quote_ident(rol.rolname) AS objname,
	l.provider, l.label
FROM
	pg_shseclabel l
	JOIN pg_authid rol ON l.classoid = rol.tableoid AND l.objoid = rol.oid;

CREATE VIEW pg_settings AS
    SELECT * FROM pg_show_all_settings() AS A;

CREATE RULE pg_settings_u AS
    ON UPDATE TO pg_settings
    WHERE new.name = old.name DO
    SELECT set_config(old.name, new.setting, 'f');

CREATE RULE pg_settings_n AS
    ON UPDATE TO pg_settings
    DO INSTEAD NOTHING;

GRANT SELECT, UPDATE ON pg_settings TO PUBLIC;

CREATE VIEW pg_file_settings AS
   SELECT * FROM pg_show_all_file_settings() AS A;

REVOKE ALL on pg_file_settings FROM PUBLIC;
REVOKE EXECUTE ON FUNCTION pg_show_all_file_settings() FROM PUBLIC;

CREATE VIEW pg_timezone_abbrevs AS
    SELECT * FROM pg_timezone_abbrevs();

CREATE VIEW pg_timezone_names AS
    SELECT * FROM pg_timezone_names();

CREATE VIEW pg_config AS
    SELECT * FROM pg_config();

REVOKE ALL on pg_config FROM PUBLIC;
REVOKE EXECUTE ON FUNCTION pg_config() FROM PUBLIC;

-- Statistics views

CREATE VIEW pg_stat_all_tables_internal AS
    SELECT
            C.oid AS relid,
            N.nspname AS schemaname,
            C.relname AS relname,
            pg_stat_get_numscans(C.oid) AS seq_scan,
            pg_stat_get_tuples_returned(C.oid) AS seq_tup_read,
            sum(pg_stat_get_numscans(I.indexrelid))::bigint AS idx_scan,
            sum(pg_stat_get_tuples_fetched(I.indexrelid))::bigint +
            pg_stat_get_tuples_fetched(C.oid) AS idx_tup_fetch,
            pg_stat_get_tuples_inserted(C.oid) AS n_tup_ins,
            pg_stat_get_tuples_updated(C.oid) AS n_tup_upd,
            pg_stat_get_tuples_deleted(C.oid) AS n_tup_del,
            pg_stat_get_tuples_hot_updated(C.oid) AS n_tup_hot_upd,
            pg_stat_get_live_tuples(C.oid) AS n_live_tup,
            pg_stat_get_dead_tuples(C.oid) AS n_dead_tup,
            pg_stat_get_mod_since_analyze(C.oid) AS n_mod_since_analyze,
            pg_stat_get_last_vacuum_time(C.oid) as last_vacuum,
            pg_stat_get_last_autovacuum_time(C.oid) as last_autovacuum,
            pg_stat_get_last_analyze_time(C.oid) as last_analyze,
            pg_stat_get_last_autoanalyze_time(C.oid) as last_autoanalyze,
            pg_stat_get_vacuum_count(C.oid) AS vacuum_count,
            pg_stat_get_autovacuum_count(C.oid) AS autovacuum_count,
            pg_stat_get_analyze_count(C.oid) AS analyze_count,
            pg_stat_get_autoanalyze_count(C.oid) AS autoanalyze_count
    FROM pg_class C LEFT JOIN
         pg_index I ON C.oid = I.indrelid
         LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
    WHERE C.relkind IN ('r', 't', 'm')
    GROUP BY C.oid, N.nspname, C.relname;

-- Gather data from segments on user tables, and use data on master on system tables.

CREATE VIEW pg_stat_all_tables AS
SELECT
    s.relid,
    s.schemaname,
    s.relname,
    m.seq_scan,
    m.seq_tup_read,
    m.idx_scan,
    m.idx_tup_fetch,
    m.n_tup_ins,
    m.n_tup_upd,
    m.n_tup_del,
    m.n_tup_hot_upd,
    m.n_live_tup,
    m.n_dead_tup,
    m.n_mod_since_analyze,
    s.last_vacuum,
    s.last_autovacuum,
    s.last_analyze,
    s.last_autoanalyze,
    s.vacuum_count,
    s.autovacuum_count,
    s.analyze_count,
    s.autoanalyze_count
FROM
    (SELECT
         relid,
         schemaname,
         relname,
         sum(seq_scan) as seq_scan,
         sum(seq_tup_read) as seq_tup_read,
         sum(idx_scan) as idx_scan,
         sum(idx_tup_fetch) as idx_tup_fetch,
         sum(n_tup_ins) as n_tup_ins,
         sum(n_tup_upd) as n_tup_upd,
         sum(n_tup_del) as n_tup_del,
         sum(n_tup_hot_upd) as n_tup_hot_upd,
         sum(n_live_tup) as n_live_tup,
         sum(n_dead_tup) as n_dead_tup,
         max(n_mod_since_analyze) as n_mod_since_analyze,
         max(last_vacuum) as last_vacuum,
         max(last_autovacuum) as last_autovacuum,
         max(last_analyze) as last_analyze,
         max(last_autoanalyze) as last_autoanalyze,
         max(vacuum_count) as vacuum_count,
         max(autovacuum_count) as autovacuum_count,
         max(analyze_count) as analyze_count,
         max(autoanalyze_count) as autoanalyze_count
     FROM
         gp_dist_random('pg_stat_all_tables_internal')
     WHERE
             relid >= 16384
     GROUP BY relid, schemaname, relname

     UNION ALL

     SELECT
         *
     FROM
         pg_stat_all_tables_internal
     WHERE
             relid < 16384) m, pg_stat_all_tables_internal s
WHERE m.relid = s.relid;

CREATE VIEW pg_stat_xact_all_tables AS
    SELECT
            C.oid AS relid,
            N.nspname AS schemaname,
            C.relname AS relname,
            pg_stat_get_xact_numscans(C.oid) AS seq_scan,
            pg_stat_get_xact_tuples_returned(C.oid) AS seq_tup_read,
            sum(pg_stat_get_xact_numscans(I.indexrelid))::bigint AS idx_scan,
            sum(pg_stat_get_xact_tuples_fetched(I.indexrelid))::bigint +
            pg_stat_get_xact_tuples_fetched(C.oid) AS idx_tup_fetch,
            pg_stat_get_xact_tuples_inserted(C.oid) AS n_tup_ins,
            pg_stat_get_xact_tuples_updated(C.oid) AS n_tup_upd,
            pg_stat_get_xact_tuples_deleted(C.oid) AS n_tup_del,
            pg_stat_get_xact_tuples_hot_updated(C.oid) AS n_tup_hot_upd
    FROM pg_class C LEFT JOIN
         pg_index I ON C.oid = I.indrelid
         LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
    WHERE C.relkind IN ('r', 't', 'm')
    GROUP BY C.oid, N.nspname, C.relname;

CREATE VIEW pg_stat_sys_tables AS
    SELECT * FROM pg_stat_all_tables
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_stat_xact_sys_tables AS
    SELECT * FROM pg_stat_xact_all_tables
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_stat_user_tables AS
    SELECT * FROM pg_stat_all_tables
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_stat_xact_user_tables AS
    SELECT * FROM pg_stat_xact_all_tables
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_statio_all_tables AS
    SELECT
            C.oid AS relid,
            N.nspname AS schemaname,
            C.relname AS relname,
            pg_stat_get_blocks_fetched(C.oid) -
                    pg_stat_get_blocks_hit(C.oid) AS heap_blks_read,
            pg_stat_get_blocks_hit(C.oid) AS heap_blks_hit,
            sum(pg_stat_get_blocks_fetched(I.indexrelid) -
                    pg_stat_get_blocks_hit(I.indexrelid))::bigint AS idx_blks_read,
            sum(pg_stat_get_blocks_hit(I.indexrelid))::bigint AS idx_blks_hit,
            pg_stat_get_blocks_fetched(T.oid) -
                    pg_stat_get_blocks_hit(T.oid) AS toast_blks_read,
            pg_stat_get_blocks_hit(T.oid) AS toast_blks_hit,
            sum(pg_stat_get_blocks_fetched(X.indexrelid) -
                    pg_stat_get_blocks_hit(X.indexrelid))::bigint AS tidx_blks_read,
            sum(pg_stat_get_blocks_hit(X.indexrelid))::bigint AS tidx_blks_hit
    FROM pg_class C LEFT JOIN
            pg_index I ON C.oid = I.indrelid LEFT JOIN
            pg_class T ON C.reltoastrelid = T.oid LEFT JOIN
            pg_index X ON T.oid = X.indrelid
            LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
    WHERE C.relkind IN ('r', 't', 'm')
    GROUP BY C.oid, N.nspname, C.relname, T.oid, X.indrelid;

CREATE VIEW pg_statio_sys_tables AS
    SELECT * FROM pg_statio_all_tables
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_statio_user_tables AS
    SELECT * FROM pg_statio_all_tables
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_stat_all_indexes_internal AS
    SELECT
            C.oid AS relid,
            I.oid AS indexrelid,
            N.nspname AS schemaname,
            C.relname AS relname,
            I.relname AS indexrelname,
            pg_stat_get_numscans(I.oid) AS idx_scan,
            pg_stat_get_tuples_returned(I.oid) AS idx_tup_read,
            pg_stat_get_tuples_fetched(I.oid) AS idx_tup_fetch
    FROM pg_class C JOIN
            pg_index X ON C.oid = X.indrelid JOIN
            pg_class I ON I.oid = X.indexrelid
            LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
    WHERE C.relkind IN ('r', 't', 'm');

-- Gather data from segments on user tables, and use data on master on system tables.

CREATE VIEW pg_stat_all_indexes AS
SELECT
    s.relid,
    s.indexrelid,
    s.schemaname,
    s.relname,
    s.indexrelname,
    m.idx_scan,
    m.idx_tup_read,
    m.idx_tup_fetch
FROM
    (SELECT
         relid,
         indexrelid,
         schemaname,
         relname,
         indexrelname,
         sum(idx_scan) as idx_scan,
         sum(idx_tup_read) as idx_tup_read,
         sum(idx_tup_fetch) as idx_tup_fetch
     FROM
         gp_dist_random('pg_stat_all_indexes_internal')
     WHERE
             relid >= 16384
     GROUP BY relid, indexrelid, schemaname, relname, indexrelname

     UNION ALL

     SELECT
         *
     FROM
         pg_stat_all_indexes_internal
     WHERE
             relid < 16384) m, pg_stat_all_indexes_internal s
WHERE m.relid = s.relid;

CREATE VIEW pg_stat_sys_indexes AS
    SELECT * FROM pg_stat_all_indexes
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_stat_user_indexes AS
    SELECT * FROM pg_stat_all_indexes
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_statio_all_indexes AS
    SELECT
            C.oid AS relid,
            I.oid AS indexrelid,
            N.nspname AS schemaname,
            C.relname AS relname,
            I.relname AS indexrelname,
            pg_stat_get_blocks_fetched(I.oid) -
                    pg_stat_get_blocks_hit(I.oid) AS idx_blks_read,
            pg_stat_get_blocks_hit(I.oid) AS idx_blks_hit
    FROM pg_class C JOIN
            pg_index X ON C.oid = X.indrelid JOIN
            pg_class I ON I.oid = X.indexrelid
            LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
    WHERE C.relkind IN ('r', 't', 'm');

CREATE VIEW pg_statio_sys_indexes AS
    SELECT * FROM pg_statio_all_indexes
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_statio_user_indexes AS
    SELECT * FROM pg_statio_all_indexes
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_statio_all_sequences AS
    SELECT
            C.oid AS relid,
            N.nspname AS schemaname,
            C.relname AS relname,
            pg_stat_get_blocks_fetched(C.oid) -
                    pg_stat_get_blocks_hit(C.oid) AS blks_read,
            pg_stat_get_blocks_hit(C.oid) AS blks_hit
    FROM pg_class C
            LEFT JOIN pg_namespace N ON (N.oid = C.relnamespace)
    WHERE C.relkind = 'S';

CREATE VIEW pg_statio_sys_sequences AS
    SELECT * FROM pg_statio_all_sequences
    WHERE schemaname IN ('pg_catalog', 'information_schema') OR
          schemaname ~ '^pg_toast';

CREATE VIEW pg_statio_user_sequences AS
    SELECT * FROM pg_statio_all_sequences
    WHERE schemaname NOT IN ('pg_catalog', 'information_schema') AND
          schemaname !~ '^pg_toast';

CREATE VIEW pg_stat_activity AS
    SELECT
            S.datid AS datid,
            D.datname AS datname,
            S.pid,
            S.sess_id,
            S.usesysid,
            U.rolname AS usename,
            S.application_name,
            S.client_addr,
            S.client_hostname,
            S.client_port,
            S.backend_start,
            S.xact_start,
            S.query_start,
            S.state_change,
            S.wait_event_type,
            S.wait_event,
            S.state,
            S.backend_xid,
            s.backend_xmin,
            S.query,

            S.rsgid,
            S.rsgname
    FROM pg_database D, pg_stat_get_activity(NULL) AS S, pg_authid U
    WHERE S.datid = D.oid AND
            S.usesysid = U.oid;

CREATE VIEW pg_stat_replication AS
    SELECT
            S.pid,
            S.usesysid,
            U.rolname AS usename,
            S.application_name,
            S.client_addr,
            S.client_hostname,
            S.client_port,
            S.backend_start,
            S.backend_xmin,
            W.state,
            W.sent_location,
            W.write_location,
            W.flush_location,
            W.replay_location,
            W.sync_priority,
            W.sync_state
    FROM pg_stat_get_activity(NULL) AS S, pg_authid U,
            pg_stat_get_wal_senders() AS W
    WHERE S.usesysid = U.oid AND
            S.pid = W.pid;

CREATE FUNCTION gp_stat_get_master_replication() RETURNS SETOF RECORD AS
$$
    SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, *
    FROM pg_catalog.pg_stat_replication
$$
LANGUAGE SQL EXECUTE ON MASTER;

CREATE FUNCTION gp_stat_get_segment_replication() RETURNS SETOF RECORD AS
$$
    SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, *
    FROM pg_catalog.pg_stat_replication
$$
LANGUAGE SQL EXECUTE ON ALL SEGMENTS;

CREATE FUNCTION gp_stat_get_segment_replication_error() RETURNS SETOF RECORD AS
$$
    SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, pg_catalog.gp_replication_error() as sync_error
$$
LANGUAGE SQL EXECUTE ON ALL SEGMENTS;

CREATE VIEW gp_stat_replication AS
    SELECT *, pg_catalog.gp_replication_error() AS sync_error
    FROM pg_catalog.gp_stat_get_master_replication() AS R
    (gp_segment_id integer, pid integer, usesysid oid,
     usename name, application_name text, client_addr inet, client_hostname text,
     client_port integer, backend_start timestamptz, backend_xmin xid, state text,
     sent_location pg_lsn, write_location pg_lsn, flush_location pg_lsn,
     replay_location pg_lsn, sync_priority integer, sync_state text)
    UNION ALL
    (
        SELECT G.gp_segment_id
            , R.pid, R.usesysid, R.usename, R.application_name, R.client_addr
            , R.client_hostname, R.client_port, R.backend_start, R.backend_xmin, R.state
	    , R.sent_location, R.write_location, R.flush_location
	    , R.replay_location, R.sync_priority, R.sync_state, G.sync_error
        FROM (
            SELECT E.*
            FROM pg_catalog.gp_segment_configuration C
            JOIN pg_catalog.gp_stat_get_segment_replication_error()
	    AS E (gp_segment_id integer, sync_error text)
            ON c.content = E.gp_segment_id
            WHERE C.role = 'm'
        ) G
        LEFT OUTER JOIN pg_catalog.gp_stat_get_segment_replication() AS R
        (gp_segment_id integer, pid integer, usesysid oid,
         usename name, application_name text, client_addr inet,
	 client_hostname text, client_port integer, backend_start timestamptz,
	 backend_xmin xid, state text, sent_location pg_lsn,
	 write_location pg_lsn, flush_location pg_lsn,
         replay_location pg_lsn, sync_priority integer, sync_state text)
         ON G.gp_segment_id = R.gp_segment_id
    );

CREATE VIEW pg_stat_wal_receiver AS
    SELECT
            s.pid,
            s.status,
            s.receive_start_lsn,
            s.receive_start_tli,
            s.received_lsn,
            s.received_tli,
            s.last_msg_send_time,
            s.last_msg_receipt_time,
            s.latest_end_lsn,
            s.latest_end_time,
            s.slot_name,
            s.conninfo
    FROM pg_stat_get_wal_receiver() s
    WHERE s.pid IS NOT NULL;

CREATE VIEW pg_stat_ssl AS
    SELECT
            S.pid,
            S.ssl,
            S.sslversion AS version,
            S.sslcipher AS cipher,
            S.sslbits AS bits,
            S.sslcompression AS compression,
            S.sslclientdn AS clientdn
    FROM pg_stat_get_activity(NULL) AS S;

CREATE VIEW pg_replication_slots AS
    SELECT
            L.slot_name,
            L.plugin,
            L.slot_type,
            L.datoid,
            D.datname AS database,
            L.active,
            L.active_pid,
            L.xmin,
            L.catalog_xmin,
            L.restart_lsn,
            L.confirmed_flush_lsn
    FROM pg_get_replication_slots() AS L
            LEFT JOIN pg_database D ON (L.datoid = D.oid);

CREATE VIEW pg_stat_database AS
    SELECT
            D.oid AS datid,
            D.datname AS datname,
            pg_stat_get_db_numbackends(D.oid) AS numbackends,
            pg_stat_get_db_xact_commit(D.oid) AS xact_commit,
            pg_stat_get_db_xact_rollback(D.oid) AS xact_rollback,
            pg_stat_get_db_blocks_fetched(D.oid) -
                    pg_stat_get_db_blocks_hit(D.oid) AS blks_read,
            pg_stat_get_db_blocks_hit(D.oid) AS blks_hit,
            pg_stat_get_db_tuples_returned(D.oid) AS tup_returned,
            pg_stat_get_db_tuples_fetched(D.oid) AS tup_fetched,
            pg_stat_get_db_tuples_inserted(D.oid) AS tup_inserted,
            pg_stat_get_db_tuples_updated(D.oid) AS tup_updated,
            pg_stat_get_db_tuples_deleted(D.oid) AS tup_deleted,
            pg_stat_get_db_conflict_all(D.oid) AS conflicts,
            pg_stat_get_db_temp_files(D.oid) AS temp_files,
            pg_stat_get_db_temp_bytes(D.oid) AS temp_bytes,
            pg_stat_get_db_deadlocks(D.oid) AS deadlocks,
            pg_stat_get_db_blk_read_time(D.oid) AS blk_read_time,
            pg_stat_get_db_blk_write_time(D.oid) AS blk_write_time,
            pg_stat_get_db_stat_reset_time(D.oid) AS stats_reset
    FROM pg_database D;

CREATE VIEW pg_stat_resqueues AS
	SELECT
		Q.oid AS queueid,
		Q.rsqname AS queuename,
		pg_stat_get_queue_num_exec(Q.oid) AS n_queries_exec,
		pg_stat_get_queue_num_wait(Q.oid) AS n_queries_wait,
		pg_stat_get_queue_elapsed_exec(Q.oid) AS elapsed_exec,
		pg_stat_get_queue_elapsed_wait(Q.oid) AS elapsed_wait
	FROM pg_resqueue AS Q;

-- Resource queue views

CREATE VIEW pg_resqueue_status AS
	SELECT 
			q.rsqname, 
			q.rsqcountlimit, 
			s.queuecountvalue AS rsqcountvalue,
			q.rsqcostlimit, 
			s.queuecostvalue AS rsqcostvalue,
			s.queuewaiters AS rsqwaiters,
			s.queueholders AS rsqholders
	FROM pg_resqueue AS q 
			INNER JOIN pg_resqueue_status() AS s 
			(	queueid oid, 
	 			queuecountvalue float4, 
				queuecostvalue float4,
				queuewaiters int4,
				queueholders int4)
			ON (s.queueid = q.oid);
			
-- External table views

CREATE VIEW pg_max_external_files AS
    SELECT   address::name as hostname, count(*) as maxfiles
    FROM     gp_segment_configuration
    WHERE    content >= 0 
    AND      role='p'
    GROUP BY address;

-- partitioning
create view pg_partitions as
  select 
      schemaname, 
      tablename, 
      partitionschemaname, 
      partitiontablename, 
      partitionname, 
      parentpartitiontablename, 
      parentpartitionname, 
      partitiontype, 
      partitionlevel, 
      -- Only the non-default parts of range partitions have 
      -- a non-null partition rank.  For these the rank is
      -- from (1, 2, ...) in keeping with the use of RANK(n)
      -- to identify the parts of a range partition in the 
      -- ALTER statement.
      case
          when partitiontype <> 'range'::text then null::bigint
          when partitionnodefault > 0 then partitionrank
          when partitionrank = 0 then null::bigint
          else partitionrank
          end as partitionrank, 
      partitionposition, 
      partitionlistvalues, 
      partitionrangestart, 
      case
          when partitiontype = 'range'::text then partitionstartinclusive
          else null::boolean
          end as partitionstartinclusive, partitionrangeend, 
      case
          when partitiontype = 'range'::text then partitionendinclusive
          else null::boolean
          end as partitionendinclusive, 
      partitioneveryclause, 
      parisdefault as partitionisdefault, 
      partitionboundary,
      parentspace as parenttablespace,
      partspace as partitiontablespace
  from 
      ( 
          select 
              n.nspname as schemaname, 
              cl.relname as tablename, 
              n2.nspname as partitionschemaname, 
              cl2.relname as partitiontablename, 
              pr1.parname as partitionname, 
              cl3.relname as parentpartitiontablename, 
              pr2.parname as parentpartitionname, 
              case
                  when pp.parkind = 'h'::"char" then 'hash'::text
                  when pp.parkind = 'r'::"char" then 'range'::text
                  when pp.parkind = 'l'::"char" then 'list'::text
                  else null::text
                  end as partitiontype, 
              pp.parlevel as partitionlevel, 
              pr1.parruleord as partitionposition, 
              case
                  when pp.parkind != 'r'::"char" or pr1.parisdefault then null::bigint
                  else
                      rank() over(
                      partition by pp.oid, cl.relname, pp.parlevel, cl3.relname
                      order by pr1.parisdefault, pr1.parruleord) 
                  end as partitionrank, 
              pg_get_expr(pr1.parlistvalues, pr1.parchildrelid, false, true) as partitionlistvalues, 
              pg_get_expr(pr1.parrangestart, pr1.parchildrelid, false, true) as partitionrangestart, 
              pr1.parrangestartincl as partitionstartinclusive, 
              pg_get_expr(pr1.parrangeend, pr1.parchildrelid, false, true) as partitionrangeend, 
              pr1.parrangeendincl as partitionendinclusive, 
              pg_get_expr(pr1.parrangeevery, pr1.parchildrelid, false, true) as partitioneveryclause, 
              min(pr1.parruleord) over(
                  partition by pp.oid, cl.relname, pp.parlevel, cl3.relname
                  order by pr1.parruleord) as partitionnodefault, 
              pr1.parisdefault, 
              pg_get_partition_rule_def(pr1.oid, true) as partitionboundary,
              coalesce(sp.spcname, dfltspcname) as parentspace,
              coalesce(sp3.spcname, dfltspcname) as partspace
          from 
              pg_namespace n, 
              pg_namespace n2, 
              pg_class cl
                  left join
              pg_tablespace sp on cl.reltablespace = sp.oid, 
              pg_class cl2
                  left join
              pg_tablespace sp3 on cl2.reltablespace = sp3.oid,
              pg_partition pp, 
              pg_partition_rule pr1
                  left join 
              pg_partition_rule pr2 on pr1.parparentrule = pr2.oid
                  left join 
              pg_class cl3 on pr2.parchildrelid = cl3.oid,
              (select s.spcname
               from pg_database, pg_tablespace s
               where datname = current_database()
                 and dattablespace = s.oid) d(dfltspcname)
      where 
          pp.paristemplate = false and 
          pp.parrelid = cl.oid and 
          pr1.paroid = pp.oid and 
          cl2.oid = pr1.parchildrelid and 
          cl.relnamespace = n.oid and 
          cl2.relnamespace = n2.oid) p1;

create view pg_partition_columns as											 
select																		  
n.nspname as schemaname,														
c.relname as tablename,														 
a.attname as columnname,														
p.parlevel as partitionlevel,												   
p.i + 1 as position_in_partition_key											
from pg_namespace n,															
pg_class c,																	 
pg_attribute a,																 
(select p.parrelid, p.parlevel, p.paratts[i] as attnum, i from pg_partition p,  
 generate_series(0,															 
				 (select max(array_upper(paratts, 1)) from pg_partition)																   
				) i
		where paratts[i] is not null
) p
where p.parrelid = c.oid and c.relnamespace = n.oid and
   p.attnum = a.attnum and a.attrelid = c.oid;

create view pg_partition_templates as
select
schemaname,
tablename, 
partitionname,
partitiontype, 
partitionlevel,
-- if not a range partition, no partition rank
-- for range partitions, the parruleord of the default partition is zero,
-- so if no_default (min of parruleord) > 0 then there is no default partition
-- so return the normal rank.  However, if there is a default partition, it
-- is rank 1, so skip it, and decrement remaining ranks by 1 so the first
-- non-default partition starts at 1
--
case when (partitiontype != 'range') then NULL
	 when (partitionnodefault > 0) then partitionrank
	 when (partitionrank = 1) then NULL
	 else  partitionrank - 1
end as partitionrank,
partitionposition,
partitionlistvalues,
partitionrangestart,
case when (partitiontype = 'range') then partitionstartinclusive
	 else NULL
end as partitionstartinclusive,
partitionrangeend,
case when (partitiontype = 'range') then partitionendinclusive
	else NULL
end as partitionendinclusive,
partitioneveryclause,
parisdefault as partitionisdefault,
partitionboundary
from (
select
n.nspname as schemaname,
cl.relname as tablename,
pr1.parname as partitionname,
p.parlevel as partitionlevel,
pr1.parruleord as partitionposition,
rank() over (partition by p.oid, cl.relname, p.parlevel 
			 order by pr1.parruleord) as partitionrank,
pg_get_expr(pr1.parlistvalues, p.parrelid, false, true) as partitionlistvalues,
pg_get_expr(pr1.parrangestart, p.parrelid, false, true) as partitionrangestart,
pr1.parrangestartincl as partitionstartinclusive,
pg_get_expr(pr1.parrangeend, p.parrelid, false, true) as partitionrangeend,
pr1.parrangeendincl as partitionendinclusive,
pg_get_expr(pr1.parrangeevery, p.parrelid, false, true) as partitioneveryclause,

min(pr1.parruleord) over (partition by p.oid, cl.relname, p.parlevel
	order by pr1.parruleord) as partitionnodefault,
pr1.parisdefault,
case when p.parkind = 'h' then 'hash' when p.parkind = 'r' then 'range'
	 when p.parkind = 'l' then 'list' else null end as partitiontype, 
pg_get_partition_rule_def(pr1.oid, true) as partitionboundary
from pg_namespace n, pg_class cl, pg_partition p, pg_partition_rule pr1
where 
 p.parrelid = cl.oid and 
 pr1.paroid = p.oid and
 cl.relnamespace = n.oid and
 p.paristemplate = 't'
 ) p1;

-- metadata tracking
CREATE VIEW pg_stat_operations
AS
SELECT 
'pg_authid' AS classname, 
a.rolname AS objname, 
c.objid, NULL AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename)
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime 
FROM 
pg_authid a, 
(pg_authid b FULL JOIN
pg_stat_last_shoperation c ON ((b.oid = c.stasysid))) WHERE ((a.oid
= c.objid) AND (c.classid = (SELECT pg_class.oid FROM pg_class WHERE
(pg_class.relname = 'pg_authid'::name))))
UNION 
SELECT 
'pg_class' AS classname, 
a.relname AS objname, 
c.objid,  N.nspname AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename) 
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime 
FROM pg_class
a, pg_namespace n, (pg_authid b FULL JOIN 
pg_stat_last_operation c ON ((b.oid =
c.stasysid))) WHERE 
a.relnamespace = n.oid AND
((a.oid = c.objid) AND (c.classid = (SELECT
pg_class.oid FROM pg_class WHERE ((pg_class.relname =
'pg_class'::name) AND (pg_class.relnamespace = (SELECT
pg_namespace.oid FROM pg_namespace WHERE (pg_namespace.nspname =
'pg_catalog'::name)))))))
UNION
SELECT
'pg_namespace' AS classname, a.nspname AS objname, 
c.objid,  NULL AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename)
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime
FROM pg_namespace a, (pg_authid b FULL JOIN pg_stat_last_operation c ON
((b.oid = c.stasysid))) WHERE ((a.oid = c.objid) AND (c.classid =
(SELECT pg_class.oid FROM pg_class WHERE ((pg_class.relname =
'pg_namespace'::name) AND (pg_class.relnamespace = (SELECT
pg_namespace.oid FROM pg_namespace WHERE (pg_namespace.nspname =
'pg_catalog'::name)))))))
UNION
SELECT
'pg_database' AS classname, a.datname AS objname, 
c.objid,  NULL AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename)
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime
FROM pg_database a, (pg_authid b FULL JOIN pg_stat_last_shoperation c ON
((b.oid = c.stasysid))) WHERE ((a.oid = c.objid) AND (c.classid =
(SELECT pg_class.oid FROM pg_class WHERE ((pg_class.relname =
'pg_database'::name) AND (pg_class.relnamespace = (SELECT
pg_namespace.oid FROM pg_namespace WHERE (pg_namespace.nspname =
'pg_catalog'::name)))))))
UNION 
SELECT
'pg_tablespace' AS classname, a.spcname AS objname, 
c.objid,  NULL AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename)
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime
FROM pg_tablespace a, (pg_authid b FULL JOIN pg_stat_last_shoperation c ON
((b.oid = c.stasysid))) WHERE ((a.oid = c.objid) AND (c.classid =
(SELECT pg_class.oid FROM pg_class WHERE ((pg_class.relname =
'pg_tablespace'::name) AND (pg_class.relnamespace = (SELECT
pg_namespace.oid FROM pg_namespace WHERE (pg_namespace.nspname =
'pg_catalog'::name)))))))
UNION
SELECT 'pg_resqueue' AS classname,
a.rsqname as objname,
c.objid, NULL AS schemaname,
CASE WHEN 
((b.oid = c.stasysid) AND (b.rolname = c.stausename) )
THEN 'CURRENT'
 WHEN 
(b.rolname != c.stausename)
THEN 'CHANGED'
ELSE 'DROPPED' END AS usestatus, 
CASE WHEN b.rolname IS NULL THEN c.stausename
ELSE b.rolname END AS usename, 
c.staactionname AS actionname, 
c.stasubtype AS subtype,
--
c.statime 
FROM pg_resqueue a, (pg_authid
b FULL JOIN pg_stat_last_shoperation c ON ((b.oid = c.stasysid)))
WHERE ((a.oid = c.objid) AND (c.classid = (SELECT pg_class.oid FROM
pg_class WHERE ((pg_class.relname = 'pg_resqueue'::name) AND
(pg_class.relnamespace = (SELECT pg_namespace.oid FROM pg_namespace
WHERE (pg_namespace.nspname = 'pg_catalog'::name))))))) ORDER BY 9;

CREATE VIEW
pg_stat_partition_operations
AS
SELECT pso.*,
CASE WHEN  pr.parlevel IS NOT NULL 
THEN pr.parlevel 
ELSE pr2.parlevel END AS partitionlevel,
pcns.relname AS parenttablename,
pcns.nspname AS parentschemaname,
pr.parrelid AS parent_relid
FROM
(pg_stat_operations pso
LEFT OUTER JOIN
pg_partition_rule ppr
ON pso.objid=ppr.parchildrelid
LEFT OUTER JOIN
pg_partition pr
ON pr.oid = ppr.paroid) LEFT OUTER JOIN 
--
-- only want lowest parlevel for parenttable
--
(SELECT MIN(parlevel) AS parlevel, parrelid FROM 
pg_partition prx GROUP BY parrelid ) AS pr2
ON pr2.parrelid = pso.objid
LEFT OUTER JOIN 
( SELECT pc.oid, * FROM pg_class AS pc FULL JOIN pg_namespace AS ns 
ON ns.oid = pc.relnamespace) AS pcns
ON pcns.oid = pr.parrelid
;

-- MPP-7807: show all resqueue attributes
CREATE VIEW pg_resqueue_attributes AS
SELECT rsqname, 'active_statements' AS resname,
rsqcountlimit::text AS ressetting,
1 AS restypid FROM pg_resqueue
UNION
SELECT rsqname, 'max_cost' AS resname,
rsqcostlimit::text AS ressetting,
2 AS restypid FROM pg_resqueue
UNION
SELECT rsqname, 'cost_overcommit' AS resname,
case when rsqovercommit then '1'
else '0' end AS ressetting,
4 AS restypid FROM pg_resqueue
UNION
SELECT rsqname, 'min_cost' AS resname,
rsqignorecostlimit::text AS ressetting,
3 AS restypid FROM pg_resqueue
UNION
SELECT rq.rsqname , rt.resname, rc.ressetting,
rt.restypid AS restypid FROM
pg_resqueue rq, pg_resourcetype rt,
pg_resqueuecapability rc WHERE
rq.oid=rc.resqueueid AND rc.restypid = rt.restypid
ORDER BY rsqname, restypid
;

CREATE VIEW pg_stat_database_conflicts AS
    SELECT
            D.oid AS datid,
            D.datname AS datname,
            pg_stat_get_db_conflict_tablespace(D.oid) AS confl_tablespace,
            pg_stat_get_db_conflict_lock(D.oid) AS confl_lock,
            pg_stat_get_db_conflict_snapshot(D.oid) AS confl_snapshot,
            pg_stat_get_db_conflict_bufferpin(D.oid) AS confl_bufferpin,
            pg_stat_get_db_conflict_startup_deadlock(D.oid) AS confl_deadlock
    FROM pg_database D;

CREATE VIEW pg_stat_user_functions AS
    SELECT
            P.oid AS funcid,
            N.nspname AS schemaname,
            P.proname AS funcname,
            pg_stat_get_function_calls(P.oid) AS calls,
            pg_stat_get_function_total_time(P.oid) AS total_time,
            pg_stat_get_function_self_time(P.oid) AS self_time
    FROM pg_proc P LEFT JOIN pg_namespace N ON (N.oid = P.pronamespace)
    WHERE P.prolang != 12  -- fast check to eliminate built-in functions
          AND pg_stat_get_function_calls(P.oid) IS NOT NULL;

CREATE VIEW pg_stat_xact_user_functions AS
    SELECT
            P.oid AS funcid,
            N.nspname AS schemaname,
            P.proname AS funcname,
            pg_stat_get_xact_function_calls(P.oid) AS calls,
            pg_stat_get_xact_function_total_time(P.oid) AS total_time,
            pg_stat_get_xact_function_self_time(P.oid) AS self_time
    FROM pg_proc P LEFT JOIN pg_namespace N ON (N.oid = P.pronamespace)
    WHERE P.prolang != 12  -- fast check to eliminate built-in functions
          AND pg_stat_get_xact_function_calls(P.oid) IS NOT NULL;

CREATE VIEW pg_stat_archiver AS
    SELECT
        s.archived_count,
        s.last_archived_wal,
        s.last_archived_time,
        s.failed_count,
        s.last_failed_wal,
        s.last_failed_time,
        s.stats_reset
    FROM pg_stat_get_archiver() s;

CREATE VIEW pg_stat_bgwriter AS
    SELECT
        pg_stat_get_bgwriter_timed_checkpoints() AS checkpoints_timed,
        pg_stat_get_bgwriter_requested_checkpoints() AS checkpoints_req,
        pg_stat_get_checkpoint_write_time() AS checkpoint_write_time,
        pg_stat_get_checkpoint_sync_time() AS checkpoint_sync_time,
        pg_stat_get_bgwriter_buf_written_checkpoints() AS buffers_checkpoint,
        pg_stat_get_bgwriter_buf_written_clean() AS buffers_clean,
        pg_stat_get_bgwriter_maxwritten_clean() AS maxwritten_clean,
        pg_stat_get_buf_written_backend() AS buffers_backend,
        pg_stat_get_buf_fsync_backend() AS buffers_backend_fsync,
        pg_stat_get_buf_alloc() AS buffers_alloc,
        pg_stat_get_bgwriter_stat_reset_time() AS stats_reset;

CREATE VIEW pg_stat_progress_vacuum AS
	SELECT
		S.pid AS pid, S.datid AS datid, D.datname AS datname,
		S.relid AS relid,
		CASE S.param1 WHEN 0 THEN 'initializing'
					  WHEN 1 THEN 'scanning heap'
					  WHEN 2 THEN 'vacuuming indexes'
					  WHEN 3 THEN 'vacuuming heap'
					  WHEN 4 THEN 'cleaning up indexes'
					  WHEN 5 THEN 'truncating heap'
					  WHEN 6 THEN 'performing final cleanup'
					  END AS phase,
		S.param2 AS heap_blks_total, S.param3 AS heap_blks_scanned,
		S.param4 AS heap_blks_vacuumed, S.param5 AS index_vacuum_count,
		S.param6 AS max_dead_tuples, S.param7 AS num_dead_tuples
    FROM pg_stat_get_progress_info('VACUUM') AS S
		 JOIN pg_database D ON S.datid = D.oid;

CREATE VIEW pg_user_mappings AS
    SELECT
        U.oid       AS umid,
        S.oid       AS srvid,
        S.srvname   AS srvname,
        U.umuser    AS umuser,
        CASE WHEN U.umuser = 0 THEN
            'public'
        ELSE
            A.rolname
        END AS usename,
        CASE WHEN (U.umuser <> 0 AND A.rolname = current_user
                     AND (pg_has_role(S.srvowner, 'USAGE')
                          OR has_server_privilege(S.oid, 'USAGE')))
                    OR (U.umuser = 0 AND pg_has_role(S.srvowner, 'USAGE'))
                    OR (SELECT rolsuper FROM pg_authid WHERE rolname = current_user)
                    THEN U.umoptions
                 ELSE NULL END AS umoptions
    FROM pg_user_mapping U
         LEFT JOIN pg_authid A ON (A.oid = U.umuser) JOIN
        pg_foreign_server S ON (U.umserver = S.oid);

REVOKE ALL on pg_user_mapping FROM public;


CREATE VIEW pg_replication_origin_status AS
    SELECT *
    FROM pg_show_replication_origin_status();

REVOKE ALL ON pg_replication_origin_status FROM public;

--
-- We have a few function definitions in here, too.
-- At some point there might be enough to justify breaking them out into
-- a separate "system_functions.sql" file.
--

-- Tsearch debug function.  Defined here because it'd be pretty unwieldy
-- to put it into pg_proc.h

CREATE FUNCTION ts_debug(IN config regconfig, IN document text,
    OUT alias text,
    OUT description text,
    OUT token text,
    OUT dictionaries regdictionary[],
    OUT dictionary regdictionary,
    OUT lexemes text[])
RETURNS SETOF record AS
$$
SELECT
    tt.alias AS alias,
    tt.description AS description,
    parse.token AS token,
    ARRAY ( SELECT m.mapdict::pg_catalog.regdictionary
            FROM pg_catalog.pg_ts_config_map AS m
            WHERE m.mapcfg = $1 AND m.maptokentype = parse.tokid
            ORDER BY m.mapseqno )
    AS dictionaries,
    ( SELECT mapdict::pg_catalog.regdictionary
      FROM pg_catalog.pg_ts_config_map AS m
      WHERE m.mapcfg = $1 AND m.maptokentype = parse.tokid
      ORDER BY pg_catalog.ts_lexize(mapdict, parse.token) IS NULL, m.mapseqno
      LIMIT 1
    ) AS dictionary,
    ( SELECT pg_catalog.ts_lexize(mapdict, parse.token)
      FROM pg_catalog.pg_ts_config_map AS m
      WHERE m.mapcfg = $1 AND m.maptokentype = parse.tokid
      ORDER BY pg_catalog.ts_lexize(mapdict, parse.token) IS NULL, m.mapseqno
      LIMIT 1
    ) AS lexemes
FROM pg_catalog.ts_parse(
        (SELECT cfgparser FROM pg_catalog.pg_ts_config WHERE oid = $1 ), $2
    ) AS parse,
     pg_catalog.ts_token_type(
        (SELECT cfgparser FROM pg_catalog.pg_ts_config WHERE oid = $1 )
    ) AS tt
WHERE tt.tokid = parse.tokid
$$
LANGUAGE SQL STRICT STABLE PARALLEL SAFE;

COMMENT ON FUNCTION ts_debug(regconfig,text) IS
    'debug function for text search configuration';

CREATE FUNCTION ts_debug(IN document text,
    OUT alias text,
    OUT description text,
    OUT token text,
    OUT dictionaries regdictionary[],
    OUT dictionary regdictionary,
    OUT lexemes text[])
RETURNS SETOF record AS
$$
    SELECT * FROM pg_catalog.ts_debug( pg_catalog.get_current_ts_config(), $1);
$$
LANGUAGE SQL STRICT STABLE PARALLEL SAFE;

COMMENT ON FUNCTION ts_debug(text) IS
    'debug function for current text search configuration';

--
-- Redeclare built-in functions that need default values attached to their
-- arguments.  It's impractical to set those up directly in pg_proc.h because
-- of the complexity and platform-dependency of the expression tree
-- representation.  (Note that internal functions still have to have entries
-- in pg_proc.h; we are merely causing their proargnames and proargdefaults
-- to get filled in.)
--

CREATE OR REPLACE FUNCTION
  pg_start_backup(label text, fast boolean DEFAULT false, exclusive boolean DEFAULT true)
  RETURNS pg_lsn STRICT VOLATILE LANGUAGE internal AS 'pg_start_backup'
  PARALLEL RESTRICTED;

-- legacy definition for compatibility with 9.3
CREATE OR REPLACE FUNCTION
  json_populate_record(base anyelement, from_json json, use_json_as_text boolean DEFAULT false)
  RETURNS anyelement LANGUAGE internal STABLE AS 'json_populate_record' PARALLEL SAFE;

-- legacy definition for compatibility with 9.3
CREATE OR REPLACE FUNCTION
  json_populate_recordset(base anyelement, from_json json, use_json_as_text boolean DEFAULT false)
  RETURNS SETOF anyelement LANGUAGE internal STABLE ROWS 100  AS 'json_populate_recordset' PARALLEL SAFE;

CREATE OR REPLACE FUNCTION pg_logical_slot_get_changes(
    IN slot_name name, IN upto_lsn pg_lsn, IN upto_nchanges int, VARIADIC options text[] DEFAULT '{}',
    OUT location pg_lsn, OUT xid xid, OUT data text)
RETURNS SETOF RECORD
LANGUAGE INTERNAL
VOLATILE ROWS 1000 COST 1000
AS 'pg_logical_slot_get_changes';

CREATE OR REPLACE FUNCTION pg_logical_slot_peek_changes(
    IN slot_name name, IN upto_lsn pg_lsn, IN upto_nchanges int, VARIADIC options text[] DEFAULT '{}',
    OUT location pg_lsn, OUT xid xid, OUT data text)
RETURNS SETOF RECORD
LANGUAGE INTERNAL
VOLATILE ROWS 1000 COST 1000
AS 'pg_logical_slot_peek_changes';

CREATE OR REPLACE FUNCTION pg_logical_slot_get_binary_changes(
    IN slot_name name, IN upto_lsn pg_lsn, IN upto_nchanges int, VARIADIC options text[] DEFAULT '{}',
    OUT location pg_lsn, OUT xid xid, OUT data bytea)
RETURNS SETOF RECORD
LANGUAGE INTERNAL
VOLATILE ROWS 1000 COST 1000
AS 'pg_logical_slot_get_binary_changes';

CREATE OR REPLACE FUNCTION pg_logical_slot_peek_binary_changes(
    IN slot_name name, IN upto_lsn pg_lsn, IN upto_nchanges int, VARIADIC options text[] DEFAULT '{}',
    OUT location pg_lsn, OUT xid xid, OUT data bytea)
RETURNS SETOF RECORD
LANGUAGE INTERNAL
VOLATILE ROWS 1000 COST 1000
AS 'pg_logical_slot_peek_binary_changes';

CREATE OR REPLACE FUNCTION pg_create_physical_replication_slot(
    IN slot_name name, IN immediately_reserve boolean DEFAULT false,
    OUT slot_name name, OUT xlog_position pg_lsn)
RETURNS RECORD
LANGUAGE INTERNAL
STRICT VOLATILE
AS 'pg_create_physical_replication_slot';

CREATE OR REPLACE FUNCTION
  make_interval(years int4 DEFAULT 0, months int4 DEFAULT 0, weeks int4 DEFAULT 0,
                days int4 DEFAULT 0, hours int4 DEFAULT 0, mins int4 DEFAULT 0,
                secs double precision DEFAULT 0.0)
RETURNS interval
LANGUAGE INTERNAL
STRICT IMMUTABLE PARALLEL SAFE
AS 'make_interval';

CREATE OR REPLACE FUNCTION
  jsonb_set(jsonb_in jsonb, path text[] , replacement jsonb,
            create_if_missing boolean DEFAULT true)
RETURNS jsonb
LANGUAGE INTERNAL
STRICT IMMUTABLE PARALLEL SAFE
AS 'jsonb_set';

-- pg_tablespace_location wrapper functions to see Greenplum cluster-wide tablespace locations
CREATE FUNCTION gp_tablespace_segment_location (IN tblspc_oid oid, OUT gp_segment_id int, OUT tblspc_loc text)
AS 'SELECT pg_catalog.gp_execution_segment() as gp_segment_id, * FROM pg_catalog.pg_tablespace_location($1)'
LANGUAGE SQL EXECUTE ON ALL SEGMENTS;

CREATE FUNCTION gp_tablespace_location (IN tblspc_oid oid, OUT gp_segment_id int, OUT tblspc_loc text)
RETURNS SETOF RECORD
AS
  'SELECT * FROM pg_catalog.gp_tablespace_segment_location($1)
   UNION ALL
   SELECT pg_catalog.gp_execution_segment() as gp_segment_id, * FROM pg_catalog.pg_tablespace_location($1)'
LANGUAGE SQL EXECUTE ON MASTER;

CREATE OR REPLACE FUNCTION
  parse_ident(str text, strict boolean DEFAULT true)
RETURNS text[]
LANGUAGE INTERNAL
STRICT IMMUTABLE PARALLEL SAFE
AS 'parse_ident';

CREATE OR REPLACE FUNCTION
  jsonb_insert(jsonb_in jsonb, path text[] , replacement jsonb,
            insert_after boolean DEFAULT false)
RETURNS jsonb
LANGUAGE INTERNAL
STRICT IMMUTABLE PARALLEL SAFE
AS 'jsonb_insert';

-- The default permissions for functions mean that anyone can execute them.
-- A number of functions shouldn't be executable by just anyone, but rather
-- than use explicit 'superuser()' checks in those functions, we use the GRANT
-- system to REVOKE access to those functions at initdb time.  Administrators
-- can later change who can access these functions, or leave them as only
-- available to superuser / cluster owner, if they choose.
REVOKE EXECUTE ON FUNCTION pg_start_backup(text, boolean, boolean) FROM public;
REVOKE EXECUTE ON FUNCTION pg_stop_backup() FROM public;
REVOKE EXECUTE ON FUNCTION pg_stop_backup(boolean) FROM public;
REVOKE EXECUTE ON FUNCTION pg_create_restore_point(text) FROM public;
REVOKE EXECUTE ON FUNCTION pg_switch_xlog() FROM public;
REVOKE EXECUTE ON FUNCTION pg_xlog_replay_pause() FROM public;
REVOKE EXECUTE ON FUNCTION pg_xlog_replay_resume() FROM public;
REVOKE EXECUTE ON FUNCTION pg_rotate_logfile() FROM public;
REVOKE EXECUTE ON FUNCTION pg_reload_conf() FROM public;

REVOKE EXECUTE ON FUNCTION pg_stat_reset() FROM public;
REVOKE EXECUTE ON FUNCTION pg_stat_reset_shared(text) FROM public;
REVOKE EXECUTE ON FUNCTION pg_stat_reset_single_table_counters(oid) FROM public;
REVOKE EXECUTE ON FUNCTION pg_stat_reset_single_function_counters(oid) FROM public;

create or replace function brin_summarize_new_values(t regclass) returns bigint as
$$
select sum(t.n) from (select brin_summarize_new_values_internal(t) as n from gp_dist_random('gp_id')) t;
$$
LANGUAGE sql READS SQL DATA EXECUTE ON MASTER;
