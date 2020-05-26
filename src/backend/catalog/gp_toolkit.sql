/*
 * Greenplum system views and functions.
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 */

BEGIN;

CREATE SCHEMA gp_toolkit;

GRANT USAGE ON SCHEMA gp_toolkit TO public;

--------------------------------------------------------------------------------
-- Auxiliary functions & views
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.__gp_is_append_only
--
-- @doc:
--        Determines if a table is an AOT; returns true if OID refers to an AOT,
--        false if OID refers to a non-AOT relation; empty rowset if OID is invalid
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.__gp_is_append_only
AS
    SELECT
        pgc.oid AS iaooid,
        CASE
            WHEN pgao.relid IS NULL THEN false ELSE true
        END
        AS iaotype
    FROM
        pg_catalog.pg_class pgc

        LEFT JOIN pg_catalog.pg_appendonly pgao ON (pgc.oid = pgao.relid);

GRANT SELECT ON TABLE gp_toolkit.__gp_is_append_only TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.__gp_fullname
--
-- @doc:
--        Constructs fully qualified names
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.__gp_fullname
AS
    SELECT
        pgc.oid AS fnoid,
        nspname AS fnnspname,
        relname AS fnrelname
    FROM
        pg_catalog.pg_class pgc,
        pg_catalog.pg_namespace pgn
    WHERE pgc.relnamespace = pgn.oid;

GRANT SELECT ON TABLE gp_toolkit.__gp_fullname TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.__gp_user_namespaces
--
-- @doc:
--        Shorthand for namespaces that contain user data
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.__gp_user_namespaces
AS
    SELECT
        oid as aunoid,
        nspname as aunnspname
    FROM
        pg_catalog.pg_namespace
    WHERE
        nspname NOT LIKE 'pg_%'
    AND nspname <> 'gp_toolkit'
    AND nspname <> 'information_schema';

GRANT SELECT ON TABLE gp_toolkit.__gp_user_namespaces TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.__gp_user_tables
--
-- @doc:
--        Shorthand for tables in user namespaces
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.__gp_user_tables
AS
    SELECT
        fn.fnnspname as autnspname,
        fn.fnrelname as autrelname,
        relkind as autrelkind,
        reltuples as autreltuples,
        relpages as autrelpages,
        relacl as autrelacl,
        pgc.oid as autoid,
        pgc.reltoastrelid as auttoastoid,
        pgc.relstorage as autrelstorage
    FROM
        pg_catalog.pg_class pgc,
        gp_toolkit.__gp_fullname fn
    WHERE pgc.relnamespace IN
    (
        SELECT aunoid
        FROM gp_toolkit.__gp_user_namespaces
    )
    AND (pgc.relkind = 'r' OR pgc.relkind = 'm')
    AND pgc.relispopulated = 't'
    AND pgc.oid = fn.fnoid;

GRANT SELECT ON TABLE gp_toolkit.__gp_user_tables TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.__gp_user_data_tables
--
-- @doc:
--        Shorthand for tables in user namespaces that may hold data
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.__gp_user_data_tables
AS
    SELECT aut.*
    FROM
        gp_toolkit.__gp_user_tables aut
    LEFT OUTER JOIN
        pg_catalog.pg_partition pgp
    ON aut.autoid = pgp.parrelid
    WHERE pgp.parrelid IS NULL;

GRANT SELECT ON TABLE gp_toolkit.__gp_user_data_tables TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.__gp_user_data_tables_readable
--
-- @doc:
--        Shorthand for tables in user namespaces that may hold data and are
--        readable by current user
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.__gp_user_data_tables_readable
AS
    SELECT *
    FROM gp_toolkit.__gp_user_tables aut
    WHERE has_table_privilege(aut.autoid, 'select');

GRANT SELECT ON TABLE gp_toolkit.__gp_user_data_tables_readable TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.__gp_number_of_segments
--
-- @doc:
--        Determines number of segments in a system
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.__gp_number_of_segments
AS
    SELECT
        count(*)::smallint as numsegments
    FROM
        pg_catalog.gp_segment_configuration
    WHERE
        preferred_role = 'p'
        AND content >= 0;

GRANT SELECT ON TABLE gp_toolkit.__gp_number_of_segments TO public;


CREATE EXTENSION gp_exttable_fdw;

--------------------------------------------------------------------------------
-- log-reading external tables and views
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- @table:
--        gp_toolkit.gp_log_segment_ext
--
-- @doc:
--        External table to read segment log; requires superuser privilege
--
--------------------------------------------------------------------------------
CREATE EXTERNAL WEB TABLE gp_toolkit.__gp_log_segment_ext
(
    logtime timestamp with time zone,
    loguser text,
    logdatabase text,
    logpid text,
    logthread text,
    loghost text,
    logport text,
    logsessiontime timestamp with time zone,
    logtransaction int,
    logsession text,
    logcmdcount text,
    logsegment text,
    logslice text,
    logdistxact text,
    loglocalxact text,
    logsubxact text,
    logseverity text,
    logstate text,
    logmessage text,
    logdetail text,
    loghint text,
    logquery text,
    logquerypos int,
    logcontext text,
    logdebug text,
    logcursorpos int,
    logfunction text,
    logfile text,
    logline int,
    logstack text
)
EXECUTE E'cat $GP_SEG_DATADIR/pg_log/*.csv'
FORMAT 'CSV' (DELIMITER AS ',' NULL AS '' QUOTE AS '"');

REVOKE ALL ON TABLE gp_toolkit.__gp_log_segment_ext FROM public;


--------------------------------------------------------------------------------
-- @table:
--        gp_toolkit.gp_log_master
--
-- @doc:
--        External table to read the master log; requires superuser privilege
--
--------------------------------------------------------------------------------
CREATE EXTERNAL WEB TABLE gp_toolkit.__gp_log_master_ext
(
    logtime timestamp with time zone,
    loguser text,
    logdatabase text,
    logpid text,
    logthread text,
    loghost text,
    logport text,
    logsessiontime timestamp with time zone,
    logtransaction int,
    logsession text,
    logcmdcount text,
    logsegment text,
    logslice text,
    logdistxact text,
    loglocalxact text,
    logsubxact text,
    logseverity text,
    logstate text,
    logmessage text,
    logdetail text,
    loghint text,
    logquery text,
    logquerypos int,
    logcontext text,
    logdebug text,
    logcursorpos int,
    logfunction text,
    logfile text,
    logline int,
    logstack text
)
EXECUTE E'cat $GP_SEG_DATADIR/pg_log/*.csv' ON MASTER
FORMAT 'CSV' (DELIMITER AS ',' NULL AS '' QUOTE AS '"');

REVOKE ALL ON TABLE gp_toolkit.__gp_log_master_ext FROM public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_log_system
--
-- @doc:
--        View of segment and master logs
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_log_system
AS
    SELECT * FROM gp_toolkit.__gp_log_segment_ext
    UNION ALL
    SELECT * FROM gp_toolkit.__gp_log_master_ext
    ORDER BY logtime;

REVOKE ALL ON TABLE gp_toolkit.gp_log_system FROM public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_log_database
--
-- @doc:
--        Shorthand to view error logs of current database only;
--        requires superuser privilege
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_log_database
AS
    SELECT * FROM gp_toolkit.gp_log_system
    WHERE logdatabase = current_database();

REVOKE ALL ON TABLE gp_toolkit.gp_log_database FROM public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_log_master_concise
--
-- @doc:
--        Shorthand to view most important columns of master log only;
--        requires superuser privilege
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_log_master_concise
AS
    SELECT
        logtime
--        ,loguser
        ,logdatabase
--        ,logpid
--        ,logthread
--        ,loghost
--        ,logport
--        ,logsessiontime
--        ,logtransaction
        ,logsession
        ,logcmdcount
--        ,logsegment
--        ,logslice
--        ,logdistxact
--        ,loglocalxact
--        ,logsubxact
        ,logseverity
--        ,logstate
        ,logmessage
--        ,logdetail
--        ,loghint
--        ,logquery
--        ,logquerypos
--        ,logcontext
--        ,logdebug
--        ,logcursorpos
--        ,logfunction
--        ,logfile
--        ,logline
--        ,logstack
    FROM gp_toolkit.__gp_log_master_ext;

REVOKE ALL ON TABLE gp_toolkit.gp_log_master_concise FROM public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_log_command_timings
--
-- @doc:
--        list all commands together with first and last timestamp of logged
--        activity; requires superuser privilege
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_log_command_timings
AS
    SELECT
        logsession,
        logcmdcount,
        logdatabase,
        loguser,
        logpid,
        MIN(logtime) AS logtimemin,
        MAX(logtime) AS logtimemax,
        MAX(logtime) - MIN(logtime) AS logduration
    FROM
        gp_toolkit.__gp_log_master_ext
    WHERE
        logsession IS NOT NULL
        AND logcmdcount IS NOT NULL
        AND logdatabase IS NOT NULL
    GROUP BY 1,2,3,4,5;

REVOKE ALL ON TABLE gp_toolkit.gp_log_command_timings FROM public;


--------------------------------------------------------------------------------
-- PARAM specific views
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- @type:
--        gp_toolkit.gp_param_setting_t
--
-- @doc:
--        Record type to combine segment id, param's name and param's value
--
--------------------------------------------------------------------------------
CREATE TYPE gp_toolkit.gp_param_setting_t
AS
(
    paramsegment int,
    paramname text,
    paramvalue text
);


--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.gp_param_setting
-- @in:
--        varchar - name of PARAM
-- @out:
--        int - segment id
--        text - name of PARAM
--        text - value of PARAM
--
-- @doc:
--        Collect value of a PARAM from master and all segments
--
--------------------------------------------------------------------------------
CREATE FUNCTION gp_toolkit.__gp_param_setting_on_segments(varchar)
RETURNS SETOF gp_toolkit.gp_param_setting_t
AS
$$
    SELECT gp_execution_segment(), $1, current_setting($1);
$$
LANGUAGE SQL
VOLATILE CONTAINS SQL EXECUTE ON ALL SEGMENTS;

GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_param_setting_on_segments(varchar) TO public;

CREATE FUNCTION gp_toolkit.__gp_param_setting_on_master(varchar)
RETURNS SETOF gp_toolkit.gp_param_setting_t
AS
$$
    SELECT gp_execution_segment(), $1, current_setting($1);
$$
LANGUAGE SQL
VOLATILE CONTAINS SQL EXECUTE ON MASTER;

GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_param_setting_on_master(varchar) TO public;

CREATE FUNCTION gp_toolkit.gp_param_setting(varchar)
RETURNS SETOF gp_toolkit.gp_param_setting_t
AS
$$
  SELECT * FROM gp_toolkit.__gp_param_setting_on_master($1)
  UNION ALL
  SELECT * FROM gp_toolkit.__gp_param_setting_on_segments($1);
$$
LANGUAGE SQL READS SQL DATA EXECUTE ON MASTER;

GRANT EXECUTE ON FUNCTION gp_toolkit.gp_param_setting(varchar) TO public;


--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.gp_param_settings
--
-- @doc:
--        Collect values of all parameters from all segments
--
--------------------------------------------------------------------------------
CREATE FUNCTION gp_toolkit.gp_param_settings()
RETURNS SETOF gp_toolkit.gp_param_setting_t
AS
$$
    select gp_execution_segment(), name, setting from pg_settings;
$$
LANGUAGE SQL READS SQL DATA EXECUTE ON ALL SEGMENTS;

GRANT EXECUTE ON FUNCTION gp_toolkit.gp_param_settings() TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_param_settings_seg_value_diffs
--
-- @doc:
--        Show parameters that do not have same values on all segments
--        (parameters that are supposed to have different values are excluded)
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_param_settings_seg_value_diffs
AS
    SELECT
        paramname  AS psdname,
        paramvalue AS psdvalue,
        count(*)   AS psdcount
    FROM
        gp_toolkit.gp_param_settings()
    WHERE
        paramname NOT IN ('config_file', 'data_directory', 'gp_contentid', 'gp_dbid', 'hba_file', 'ident_file', 'port')
    GROUP BY
        1,2
    HAVING
        count(*) < (select numsegments from gp_toolkit.__gp_number_of_segments)
    ORDER BY
        1,2,3;


GRANT SELECT ON TABLE gp_toolkit.gp_param_settings_seg_value_diffs TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_pgdatabase_invalid
--
-- @doc:
--        Information about the invalid segments only
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_pgdatabase_invalid
AS
    SELECT
        dbid           AS pgdbidbid,
        isprimary      AS pgdbiisprimary,
        content        AS pgdbicontent,
        valid          AS pgdbivalid,
        definedprimary AS pgdbidefinedprimary
    FROM
        pg_catalog.gp_pgdatabase
    WHERE
        not valid
    ORDER BY
        dbid;

GRANT SELECT ON TABLE gp_toolkit.gp_pgdatabase_invalid TO public;


--------------------------------------------------------------------------------
-- skew analysis
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- @type:
--        gp_toolkit.gp_skew_details_t
--
-- @doc:
--        Type to accommodate skew details
--
--------------------------------------------------------------------------------
CREATE TYPE gp_toolkit.gp_skew_details_t
AS
(
    segoid oid,
    segid int,
    segtupcount bigint
);

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.gp_skew_details
-- @in:
--        oid - oid of table for which to determine distribution
-- @out:
--        oid - input oid
--        int - segment id
--        bigint - number of rows on segment
--
-- @doc:
--        Get details for skew
--
--------------------------------------------------------------------------------
CREATE FUNCTION gp_toolkit.gp_skew_details(oid)
RETURNS setof gp_toolkit.gp_skew_details_t
AS
$$
DECLARE
    skewcrs refcursor;
    skewrec record;
    skewarray bigint[];
    skewaot bool;
    skewsegid int;
    skewtablename record;
    skewreplicated record;
BEGIN
    SELECT INTO skewrec *
    FROM pg_catalog.pg_appendonly pga, pg_catalog.pg_roles pgr
    WHERE pga.relid = $1::regclass and pgr.rolname = current_user and pgr.rolsuper = 't';
    IF FOUND THEN
        -- append only table
        FOR skewrec IN
            SELECT $1, segid, COALESCE(tupcount, 0)::bigint AS cnt
            FROM (SELECT generate_series(0, numsegments - 1) FROM gp_toolkit.__gp_number_of_segments) segs(segid)
            LEFT OUTER JOIN pg_catalog.get_ao_distribution($1)
            ON segid = segmentid
        LOOP
            RETURN NEXT skewrec;
        END LOOP;
    ELSE
        -- heap table
        SELECT * INTO skewtablename FROM gp_toolkit.__gp_fullname
        WHERE fnoid = $1;
        SELECT * INTO skewreplicated FROM gp_distribution_policy WHERE policytype = 'r' AND localoid = $1;
        IF FOUND THEN
            -- replicated table, gp_segment_id is user-invisible and all replicas have same count of tuples.
            OPEN skewcrs
                FOR
                EXECUTE
                    'SELECT ' || $1 || '::oid, segid, ' ||
                    '(' ||
                        'SELECT COUNT(*) AS cnt FROM ' ||
                        quote_ident(skewtablename.fnnspname) ||
                        '.' ||
                        quote_ident(skewtablename.fnrelname) ||
                    ') '
                    'FROM (SELECT generate_series(0, numsegments - 1) FROM gp_toolkit.__gp_number_of_segments) segs(segid)';
        ELSE
            OPEN skewcrs
                FOR
                EXECUTE
                    'SELECT ' || $1 || '::oid, segid, CASE WHEN gp_segment_id IS NULL THEN 0 ELSE cnt END ' ||
                    'FROM (SELECT generate_series(0, numsegments - 1) FROM gp_toolkit.__gp_number_of_segments) segs(segid) ' ||
                    'LEFT OUTER JOIN ' ||
                        '(SELECT gp_segment_id, COUNT(*) AS cnt FROM ' ||
                            quote_ident(skewtablename.fnnspname) ||
                            '.' ||
                            quote_ident(skewtablename.fnrelname) ||
                        ' GROUP BY 1) details ' ||
                    'ON segid = gp_segment_id';
        END IF;
        FOR skewsegid IN
            SELECT generate_series(1, numsegments)
            FROM gp_toolkit.__gp_number_of_segments
        LOOP
            FETCH skewcrs INTO skewrec;
            IF FOUND THEN
                RETURN NEXT skewrec;
            ELSE
                RETURN;
            END IF;
        END LOOP;
        CLOSE skewcrs;
    END IF;
    RETURN;
END;
$$
LANGUAGE plpgsql READS SQL DATA;

GRANT EXECUTE ON FUNCTION gp_toolkit.gp_skew_details(oid) TO public;


--------------------------------------------------------------------------------
-- @type:
--        gp_toolkit.gp_skew_analysis_t
--
-- @doc:
--        Type to accommodate skew analysis
--
--------------------------------------------------------------------------------
CREATE TYPE gp_toolkit.gp_skew_analysis_t
AS
(
    skewoid oid,
    skewval numeric
);


--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.gp_skew_coefficient
-- @in:
--        oid - oid of table for which to compute skew coefficient
-- @out:
--        oid - input oid
--        numeric - skew coefficient
--
-- @doc:
--        Compute coefficient of variance given an array of rowcounts;
--
--------------------------------------------------------------------------------
CREATE FUNCTION gp_toolkit.gp_skew_coefficient(targetoid oid, OUT skcoid oid, OUT skccoeff numeric)
RETURNS record
AS
$$
    SELECT
        $1 as skcoid,
        CASE
            WHEN skewmean > 0 THEN ((skewdev/skewmean) * 100.0)
            ELSE 0
        END
        AS skccoeff
    FROM
    (
        SELECT STDDEV(segtupcount) AS skewdev, AVG(segtupcount) AS skewmean, COUNT(*) AS skewcnt
        FROM gp_toolkit.gp_skew_details($1)
    ) AS skew

$$
LANGUAGE sql READS SQL DATA;

GRANT EXECUTE ON FUNCTION gp_toolkit.gp_skew_coefficient(oid, OUT oid, OUT numeric) TO public;


--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.__gp_skew_coefficients
-- @in:
-- @out:
--        oid - oid of analyzed table
--        numeric - skew coefficient of table
--
-- @doc:
--        Wrapper to call coefficient function on all user tables
--
--------------------------------------------------------------------------------
CREATE FUNCTION gp_toolkit.__gp_skew_coefficients()
RETURNS SETOF gp_toolkit.gp_skew_analysis_t
AS
$$
DECLARE
    skcoid oid;
    skcrec record;
BEGIN
    FOR skcoid IN SELECT autoid from gp_toolkit.__gp_user_data_tables_readable WHERE autrelstorage != 'x'
    LOOP
        SELECT * INTO skcrec
        FROM
            gp_toolkit.gp_skew_coefficient(skcoid);
        RETURN NEXT skcrec;
    END LOOP;
END;
$$
LANGUAGE plpgsql READS SQL DATA;

GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_skew_coefficients() TO public;


--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.gp_skew_coefficients
--
-- @doc:
--        Wrapper view around previous function
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_skew_coefficients
AS
    SELECT
        skew.skewoid AS skcoid,
        pgn.nspname  AS skcnamespace,
        pgc.relname  AS skcrelname,
        skew.skewval AS skccoeff
    FROM gp_toolkit.__gp_skew_coefficients() skew

    JOIN
    pg_catalog.pg_class pgc
    ON (skew.skewoid = pgc.oid)

    JOIN
    pg_catalog.pg_namespace pgn
    ON (pgc.relnamespace = pgn.oid);

GRANT SELECT ON TABLE gp_toolkit.gp_skew_coefficients TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_skew_idle_fraction
-- @in:
--        oid - oid of table for which to compute idle fraction due to skew
-- @out:
--        oid - input oid
--        numeric - idle fraction
--
-- @doc:
--        Compute skew area quotient for a given table
--
--------------------------------------------------------------------------------
CREATE FUNCTION gp_toolkit.gp_skew_idle_fraction(targetoid oid, OUT sifoid oid, OUT siffraction numeric)
RETURNS record
AS
$$
    SELECT
        $1 as sifoid,
        CASE
            WHEN MIN(skewmax) = 0 THEN 0
            ELSE (SUM(skewmax - segtupcount) / (MIN(skewmax) * MIN(numsegments)))
        END
        AS siffraction
    FROM
    (
        SELECT segid, segtupcount, COUNT(segid) OVER () AS numsegments, MAX(segtupcount) OVER () AS skewmax
        FROM gp_toolkit.gp_skew_details($1)
    ) AS skewbaseline

$$
LANGUAGE sql READS SQL DATA;

GRANT EXECUTE ON FUNCTION gp_toolkit.gp_skew_idle_fraction(oid, OUT oid, OUT numeric) TO public;


--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.__gp_skew_idle_fractions
-- @in:
-- @out:
--        oid - oid of analyzed table
--        numeric - idle fraction
--
-- @doc:
--        Wrapper to call fraction calculation on all user tables
--
--------------------------------------------------------------------------------
CREATE FUNCTION gp_toolkit.__gp_skew_idle_fractions()
RETURNS SETOF gp_toolkit.gp_skew_analysis_t
AS
$$
DECLARE
    skcoid oid;
    skcrec record;
BEGIN
    FOR skcoid IN SELECT autoid from gp_toolkit.__gp_user_data_tables_readable WHERE autrelstorage != 'x'
    LOOP
        SELECT * INTO skcrec
        FROM
            gp_toolkit.gp_skew_idle_fraction(skcoid);
        RETURN NEXT skcrec;
    END LOOP;
END;
$$
LANGUAGE plpgsql READS SQL DATA;

GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_skew_idle_fractions() TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_skew_idle_fractions
--
-- @doc:
--        Wrapper view around previous function
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_skew_idle_fractions
AS
    SELECT
        skew.skewoid AS sifoid,
        pgn.nspname  AS sifnamespace,
        pgc.relname  AS sifrelname,
        skew.skewval AS siffraction
    FROM gp_toolkit.__gp_skew_idle_fractions() skew

    JOIN
    pg_catalog.pg_class pgc
    ON (skew.skewoid = pgc.oid)

    JOIN
    pg_catalog.pg_namespace pgn
    ON (pgc.relnamespace = pgn.oid);

GRANT SELECT ON TABLE gp_toolkit.gp_skew_idle_fractions TO public;


--------------------------------------------------------------------------------
-- detection of missing stats
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_stats_missing
--
-- @doc:
--        List all tables with no or insufficient stats; includes empty tables
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_stats_missing
AS
    SELECT
        aut.autnspname as smischema,
        aut.autrelname as smitable,
        CASE WHEN aut.autrelpages = 0 OR aut.autreltuples = 0 THEN false ELSE true END AS smisize,
        attcnt AS smicols,
        COALESCE(stacnt, 0) AS smirecs
    FROM
        gp_toolkit.__gp_user_tables aut

        JOIN
        (
            SELECT attrelid, count(*) AS attcnt
            FROM pg_catalog.pg_attribute
            WHERE attnum > 0 and attisdropped = false
            GROUP BY attrelid
        ) attrs
        ON aut.autoid = attrelid

        LEFT OUTER JOIN
        (
            SELECT starelid, count(*) AS stacnt
            FROM pg_catalog.pg_statistic
            GROUP BY starelid
        ) bar
        ON aut.autoid = starelid
    WHERE aut.autrelkind = 'r'
    AND (aut.autrelpages = 0 OR aut.autreltuples = 0) OR (stacnt IS NOT NULL AND attcnt > stacnt);

GRANT SELECT ON TABLE gp_toolkit.gp_stats_missing TO public;


--------------------------------------------------------------------------------
-- Misc
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- @table:
--        gp_toolkit.gp_disk_free
--
-- @doc:
--        External table to determine free space on disk on a per-segment basis
--
--------------------------------------------------------------------------------
CREATE EXTERNAL WEB TABLE gp_toolkit.gp_disk_free
(
    dfsegment int,
    dfhostname text,
    dfdevice text,
    dfspace bigint
)
EXECUTE E'python -c "from gppylib.commands import unix; df=unix.DiskFree.get_disk_free_info_local(''token'',''$GP_SEG_DATADIR''); print ''%s, %s, %s, %s'' % (''$GP_SEGMENT_ID'', unix.getLocalHostname(), df[0], df[3])"' FORMAT 'CSV';


--------------------------------------------------------------------------------
-- Determine table bloat
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_bloat_expected_pages
--
-- @doc:
--        compute number of expected pages for given table;
--        do not attempt to model sophisticated aspects of row width -- the
--        statistics this is based on are not stable enough to allow a finer
--        granularity of modelling
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_bloat_expected_pages
AS
    SELECT
        btdrelid,
        btdrelpages,
        CASE WHEN btdexppages < numsegments
            THEN numsegments
            ELSE btdexppages
        END as btdexppages
    FROM
    (
        SELECT
            oid as btdrelid,
            pgc.relpages as btdrelpages,
            CEIL((pgc.reltuples * (25 + width))::numeric / current_setting('block_size')::numeric) AS btdexppages,
            (select numsegments from gp_toolkit.__gp_number_of_segments) as numsegments
        FROM
            (
                SELECT pgc.oid, pgc.reltuples, pgc.relpages
                FROM pg_class pgc
                WHERE NOT EXISTS
                (
                    SELECT iaooid
                    FROM gp_toolkit.__gp_is_append_only
                    WHERE iaooid = pgc.oid AND iaotype = 't'
                )
                AND NOT EXISTS
                (
                    SELECT parrelid
                    FROM pg_partition
                    WHERE parrelid = pgc.oid
                )
            )
            AS pgc
        LEFT OUTER JOIN
            (
                SELECT  starelid, SUM(stawidth * (1.0 - stanullfrac)) AS width
                FROM pg_statistic pgs
                GROUP BY 1
            )
            AS btwcols
        ON pgc.oid = btwcols.starelid
        WHERE starelid IS NOT NULL
    ) AS subq;

GRANT SELECT ON TABLE gp_toolkit.gp_bloat_expected_pages TO public;


--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.gp_bloat_diag
-- @in:
--        int - actual number of pages according to statistics
--        numeric - expected number of pages
--        bool - is AO table?
-- @out:
--        int - bloat indicator
--        text - textual version of bloat indicator
--
-- @doc:
--        diagnose table bloat based on expected and actual number of pages
--
--------------------------------------------------------------------------------
CREATE FUNCTION gp_toolkit.gp_bloat_diag(btdrelpages int, btdexppages numeric, aotable bool,
    OUT bltidx int, OUT bltdiag text)
AS
$$
    SELECT
        bloatidx,
        CASE
            WHEN bloatidx = 0
                THEN 'no bloat detected'::text
            WHEN bloatidx = 1
                THEN 'moderate amount of bloat suspected'::text
            WHEN bloatidx = 2
                THEN 'significant amount of bloat suspected'::text
            WHEN bloatidx = -1
                THEN 'diagnosis inconclusive or no bloat suspected'::text
        END AS bloatdiag
    FROM
    (
        SELECT
            CASE
                WHEN $3 = 't' THEN 0
                WHEN $1 < 10 AND $2 = 0 THEN -1
                WHEN $2 = 0 THEN 2
                WHEN $1 < $2 THEN 0
                WHEN ($1/$2)::numeric > 10 THEN 2
                WHEN ($1/$2)::numeric > 3 THEN 1
                ELSE -1
            END AS bloatidx
    ) AS bloatmapping

$$
LANGUAGE SQL READS SQL DATA;

GRANT EXECUTE ON FUNCTION gp_toolkit.gp_bloat_diag(int, numeric, bool, OUT int, OUT text) TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_bloat_diag
--
-- @doc:
--        Shorthand for running bloat diag over all tables, incl. catalog
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_bloat_diag
AS
    SELECT
        btdrelid AS bdirelid,
        fnnspname AS bdinspname,
        fnrelname AS bdirelname,
        btdrelpages AS bdirelpages,
        btdexppages AS bdiexppages,
        bltdiag(bd) AS bdidiag
    FROM
    (
        SELECT
            fn.*, beg.*,
            gp_toolkit.gp_bloat_diag(btdrelpages::int, btdexppages::numeric, iao.iaotype::bool) AS bd
        FROM
            gp_toolkit.gp_bloat_expected_pages beg,
            pg_catalog.pg_class pgc,
            gp_toolkit.__gp_fullname fn,
            gp_toolkit.__gp_is_append_only iao

        WHERE beg.btdrelid = pgc.oid
            AND pgc.oid = fn.fnoid
            AND iao.iaooid = pgc.oid
    ) as bloatsummary
    WHERE bltidx(bd) > 0;

GRANT SELECT ON TABLE gp_toolkit.gp_bloat_diag TO public;


--------------------------------------------------------------------------------
-- resource queue diagnostics
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_resq_activity
--
-- @doc:
--        View that summarizes all activity in the system wrt resource queues
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_resq_activity
AS
    SELECT
        psa.pid as resqprocpid,
        psa.usename as resqrole,
        resq.resqoid,
        resq.rsqname as resqname,
        psa.query_start as resqstart,
        CASE
            WHEN resqgranted = 'f' THEN 'waiting' ELSE 'running'
        END as resqstatus
    FROM
        pg_catalog.pg_stat_activity psa
    JOIN
    (
        SELECT
            pgrq.oid as resqoid,
            pgrq.rsqname,
            pgl.pid as resqprocid,
            pgl.granted as resqgranted
        FROM
            pg_catalog.pg_resqueue pgrq,
            pg_catalog.pg_locks pgl
        WHERE
            pgl.objid = pgrq.oid
    ) as resq
    ON resqprocid = pid
    WHERE query != '<IDLE>'
    ORDER BY resqstart;

GRANT SELECT ON TABLE gp_toolkit.gp_resq_activity TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_resq_activity_by_queue
--
-- @doc:
--        Rollup of activity view
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_resq_activity_by_queue
AS
    SELECT
        resqoid,
        resqname,
        MAX(resqstart) as resqlast,
        resqstatus,
        COUNT(*) as resqtotal
    FROM
        gp_toolkit.gp_resq_activity
    GROUP BY
        resqoid, resqname, resqstatus
    ORDER BY resqoid, resqlast;

GRANT SELECT ON TABLE gp_toolkit.gp_resq_activity_by_queue TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_resq_priority_backend
--
-- @doc:
--        Priorities associated with each backend executing currently
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_resq_priority_backend
AS
    SELECT
        session_id as rqpsession,
        command_count as rqpcommand,
        priority as rqppriority,
        weight as rqpweight
    FROM
        gp_list_backend_priorities()
            AS L(session_id int, command_count int, priority text, weight int);

GRANT SELECT ON TABLE gp_toolkit.gp_resq_priority_backend TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_resq_priority_statement
--
-- @doc:
--        Priorities associated with each statement
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_resq_priority_statement
AS
    SELECT
        psa.datname AS rqpdatname,
        psa.usename AS rqpusename,
        rpb.rqpsession,
        rpb.rqpcommand,
        rpb.rqppriority,
        rpb.rqpweight,
        psa.query AS rqpquery
    FROM
        gp_toolkit.gp_resq_priority_backend rpb
        JOIN pg_stat_activity psa ON (rpb.rqpsession = psa.sess_id)
    WHERE psa.query != '<IDLE>';

GRANT SELECT ON TABLE gp_toolkit.gp_resq_priority_statement TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_resq_role
--
-- @doc:
--        Assigned resource queue to roles
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_resq_role
AS
    SELECT
        pgr.rolname  AS rrrolname,
        pgrq.rsqname AS rrrsqname
    FROM pg_catalog.pg_roles pgr

    LEFT JOIN pg_catalog.pg_resqueue pgrq
    ON (pgr.rolresqueue = pgrq.oid)
;

GRANT SELECT ON TABLE gp_toolkit.gp_resq_role TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_locks_on_resqueue
--
-- @doc:
--        Locks of type 'resource queue' (sessions waiting because of
--        resource queue restrictions)
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_locks_on_resqueue
AS
    SELECT
        pgsa.usename      AS lorusename,
        pgrq.rsqname      AS lorrsqname,
        pgl.locktype      AS lorlocktype,
        pgl.objid         AS lorobjid,
        pgl.transactionid AS lortransaction,
        pgl.pid           AS lorpid,
        pgl.mode          AS lormode,
        pgl.granted       AS lorgranted,
        pgsa.wait_event   AS lorwaitevent,
        pgsa.wait_event_type AS lorwaiteventtype
    FROM pg_catalog.pg_stat_activity pgsa

    JOIN pg_catalog.pg_locks pgl
    ON (pgsa.pid = pgl.pid)

    JOIN pg_catalog.pg_resqueue pgrq
    ON (pgl.objid = pgrq.oid);

GRANT SELECT ON TABLE gp_toolkit.gp_locks_on_resqueue TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_locks_on_relation
--
-- @doc:
--        Locks of type 'relation' (sessions waiting because of relation locks)
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_locks_on_relation
AS
    SELECT
        pgl.locktype       AS lorlocktype,
        pgl.database       AS lordatabase,
        pgc.relname        AS lorrelname,
        pgl.relation       AS lorrelation,
        pgl.transactionid  AS lortransaction,
        pgl.pid            AS lorpid,
        pgl.mode           AS lormode,
        pgl.granted        AS lorgranted,
        pgsa.query AS lorcurrentquery
    FROM pg_catalog.pg_locks pgl

    JOIN pg_catalog.pg_class pgc
    ON (pgl.relation = pgc.oid)

    JOIN pg_catalog.pg_stat_activity pgsa
    ON (pgl.pid = pgsa.pid)

    ORDER BY
        pgc.relname
;

GRANT SELECT ON TABLE gp_toolkit.gp_locks_on_relation TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_roles_assigned
--
-- @doc:
--        Shows directly assigned roles
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_roles_assigned
AS
    SELECT
        pgr.oid      AS raroleid,
        pgr.rolname  AS rarolename,
        pgam.member  AS ramemberid,
        pgr2.rolname AS ramembername
    FROM
    pg_catalog.pg_roles pgr

    LEFT JOIN pg_catalog.pg_auth_members pgam
    ON (pgr.oid = pgam.roleid)

    LEFT JOIN pg_catalog.pg_roles pgr2
    ON (pgam.member = pgr2.oid)
;

GRANT SELECT ON TABLE gp_toolkit.gp_roles_assigned TO public;



--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_size_of_index
--
-- @doc:
--              Calculates index sizes
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_size_of_index
AS
    SELECT
        soi.soioid            AS soioid,
        soi.soitableoid        AS soitableoid,
        soi.soisize            AS soisize,
        fnidx.fnnspname     AS soiindexschemaname,
        fnidx.fnrelname     AS soiindexname,
        fntbl.fnnspname     AS soitableschemaname,
        fntbl.fnrelname     AS soitablename
    FROM
        (SELECT
            pgi.indexrelid                              AS soioid,
            pgi.indrelid                                AS soitableoid,
            pg_catalog.pg_relation_size(pgi.indexrelid) AS soisize
        FROM pg_catalog.pg_index pgi

        JOIN gp_toolkit.__gp_user_data_tables_readable ut
        ON (pgi.indrelid = ut.autoid)
        ) AS soi

    JOIN gp_toolkit.__gp_fullname fnidx
    ON (soi.soioid = fnidx.fnoid)

    JOIN gp_toolkit.__gp_fullname fntbl
    ON (soi.soitableoid = fntbl.fnoid)
    ;

GRANT SELECT ON TABLE gp_toolkit.gp_size_of_index TO public;


--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_size_of_table_disk
--
-- @doc:
--              Calculates on-disk table size
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_size_of_table_disk
AS
    SELECT
        sotd.sotdoid                AS sotdoid,
        sotd.sotdsize                AS sotdsize,
        sotd.sotdtoastsize            AS sotdtoastsize,
        sotd.sotdadditionalsize        AS sotdadditionalsize,
        fn.fnnspname                 AS sotdschemaname,
        fn.fnrelname                 AS sotdtablename
    FROM
        (SELECT
            autoid                              AS sotdoid,
            pg_catalog.pg_relation_size(autoid) AS sotdsize,
            CASE
                WHEN auttoastoid > 0
                THEN pg_catalog.pg_total_relation_size(auttoastoid)
                ELSE 0
            END
            AS sotdtoastsize,
            CASE
                WHEN ao.segrelid IS NOT NULL AND ao.segrelid > 0
                THEN pg_total_relation_size(ao.segrelid)
                ELSE 0
            END
            +
            CASE
                WHEN ao.blkdirrelid IS NOT NULL AND ao.blkdirrelid > 0
                THEN pg_total_relation_size(ao.blkdirrelid)
                ELSE 0
            END
            +
            CASE
                WHEN ao.visimaprelid IS NOT NULL AND ao.visimaprelid > 0
                THEN pg_total_relation_size(ao.visimaprelid)
                ELSE 0
            END
            AS sotdadditionalsize
        FROM
            (SELECT *
             FROM gp_toolkit.__gp_user_data_tables_readable
             WHERE autrelstorage != 'x'
            ) AS udtr

        LEFT JOIN pg_catalog.pg_appendonly ao
        ON (udtr.autoid = ao.relid)
        ) AS sotd

    JOIN gp_toolkit.__gp_fullname fn
    ON (sotd.sotdoid = fn.fnoid)
    ;

GRANT SELECT ON TABLE gp_toolkit.gp_size_of_table_disk TO public;


--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_size_of_table_uncompressed
--
-- @doc:
--              Calculates uncompressed table size for AO tables, for heap
--              tables shows disk size
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_size_of_table_uncompressed
AS
    SELECT
        sotu.sotuoid    AS sotuoid,
        sotu.sotusize    AS sotusize,
        fn.fnnspname     AS sotuschemaname,
        fn.fnrelname     AS sotutablename
    FROM
        (SELECT
            sotd.sotdoid AS sotuoid,
            CASE
                WHEN iao.iaotype
                THEN
                    CASE
                        WHEN pg_catalog.pg_relation_size(sotd.sotdoid) = 0
                        THEN 0
                        ELSE pg_catalog.pg_relation_size(sotd.sotdoid) *
                                CASE
                                    WHEN pg_catalog.get_ao_compression_ratio(sotd.sotdoid) = -1
                                    THEN NULL
                                    ELSE pg_catalog.get_ao_compression_ratio(sotd.sotdoid)
                                END
                    END
                ELSE sotd.sotdsize
            END +
            sotd.sotdtoastsize +
            sotd.sotdadditionalsize
            AS sotusize
        FROM gp_toolkit.gp_size_of_table_disk sotd

        JOIN gp_toolkit.__gp_is_append_only iao
        ON (sotd.sotdoid = iao.iaooid)
        ) AS sotu

    JOIN gp_toolkit.__gp_fullname fn
    ON (sotu.sotuoid = fn.fnoid)
    ;

REVOKE ALL ON TABLE gp_toolkit.gp_size_of_table_uncompressed FROM public;


--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_table_indexes
--
-- @doc:
--              Shows indexes that belong to a table
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_table_indexes
AS
    SELECT
        ti.tireloid         AS tireloid,
        ti.tiidxoid         AS tiidxoid,
        fntbl.fnnspname        AS titableschemaname,
        fntbl.fnrelname     AS titablename,
        fnidx.fnnspname     AS tiindexschemaname,
        fnidx.fnrelname     AS tiindexname
    FROM
        (SELECT
            pgc.oid  AS tireloid,
            pgc2.oid AS tiidxoid
        FROM pg_catalog.pg_class pgc

        JOIN pg_catalog.pg_index pgi
        ON (pgc.oid = pgi.indrelid)

        JOIN pg_catalog.pg_class pgc2
        ON (pgi.indexrelid = pgc2.oid)

        JOIN gp_toolkit.__gp_user_data_tables_readable udt
        ON (udt.autoid = pgc.oid)
        ) as ti

    JOIN gp_toolkit.__gp_fullname fntbl
    ON (ti.tireloid = fntbl.fnoid)

    JOIN gp_toolkit.__gp_fullname fnidx
    ON (ti.tiidxoid = fnidx.fnoid)
    ;


GRANT SELECT ON TABLE gp_toolkit.gp_table_indexes TO public;


--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_size_of_all_table_indexes
--
-- @doc:
--              Calculates total size of all indexes on a table
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_size_of_all_table_indexes
AS
    SELECT
        soati.soatioid        AS soatioid,
        soati.soatisize        AS soatisize,
        fn.fnnspname         AS soatischemaname,
        fn.fnrelname         AS soatitablename
    FROM
        (SELECT
            tireloid                                   AS soatioid,
            sum(pg_catalog.pg_relation_size(tiidxoid)) AS soatisize
        FROM
            gp_toolkit.gp_table_indexes ti
        GROUP BY
            soatioid
        ) AS soati

    JOIN gp_toolkit.__gp_fullname fn
    ON (soati.soatioid = fn.fnoid)
    ;

GRANT SELECT ON gp_toolkit.gp_size_of_all_table_indexes TO public;


--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_size_of_table_and_indexes_disk
--
-- @doc:
--              Calculates table disk size and index disk size
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_size_of_table_and_indexes_disk
AS
    SELECT
        sotaid.sotaidoid        AS sotaidoid,
        sotaid.sotaidtablesize    AS sotaidtablesize,
        sotaid.sotaididxsize    AS sotaididxsize,
        fn.fnnspname             AS sotaidschemaname,
        fn.fnrelname             AS sotaidtablename
    FROM
        (SELECT
            sotd.sotdoid            AS sotaidoid,
            sotd.sotdsize +
            sotd.sotdtoastsize +
            sotd.sotdadditionalsize AS sotaidtablesize,
            CASE
                WHEN soati.soatisize IS NULL THEN 0
                ELSE soati.soatisize
            END
            AS sotaididxsize
        FROM gp_toolkit.gp_size_of_table_disk sotd

        LEFT JOIN gp_toolkit.gp_size_of_all_table_indexes soati
        ON (sotd.sotdoid = soati.soatioid)
        ) AS sotaid

    JOIN gp_toolkit.__gp_fullname fn
    ON (sotaid.sotaidoid = fn.fnoid)
    ;

GRANT SELECT ON TABLE gp_toolkit.gp_size_of_table_and_indexes_disk TO public;


--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_size_of_table_and_indexes_licensing
--
-- @doc:
--              Calculates table and indexes size for licensing purposes
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_size_of_table_and_indexes_licensing
AS
    SELECT
        sotail.sotailoid                    AS sotailoid,
        sotail.sotailtablesizedisk            AS sotailtablesizedisk,
        sotail.sotailtablesizeuncompressed    AS sotailtablesizeuncompressed,
        sotail.sotailindexessize            AS sotailindexessize,
        fn.fnnspname                         AS sotailschemaname,
        fn.fnrelname                         AS sotailtablename
    FROM
        (SELECT
            sotu.sotuoid           AS sotailoid,
            sotaid.sotaidtablesize AS sotailtablesizedisk,
            sotu.sotusize          AS sotailtablesizeuncompressed,
            sotaid.sotaididxsize   AS sotailindexessize
        FROM gp_toolkit.gp_size_of_table_uncompressed sotu

        JOIN gp_toolkit.gp_size_of_table_and_indexes_disk sotaid
        ON (sotu.sotuoid = sotaid.sotaidoid)
        ) AS sotail

    JOIN gp_toolkit.__gp_fullname fn
    ON (sotail.sotailoid = fn.fnoid)
    ;

REVOKE ALL ON TABLE gp_toolkit.gp_size_of_table_and_indexes_licensing FROM public;


--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_size_of_partition_and_indexes_disk
--
-- @doc:
--              Calculates partition table disk size and partition indexes disk size
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_size_of_partition_and_indexes_disk
AS
    SELECT
        sopaid.sopaidparentoid                AS sopaidparentoid,
        sopaid.sopaidpartitionoid            AS sopaidpartitionoid,
        sopaid.sopaidpartitiontablesize        AS sopaidpartitiontablesize,
        sopaid.sopaidpartitionindexessize    AS sopaidpartitionindexessize,
        fnparent.fnnspname                     AS sopaidparentschemaname,
        fnparent.fnrelname                     AS sopaidparenttablename,
        fnpart.fnnspname                     AS sopaidpartitionschemaname,
        fnpart.fnrelname                     AS sopaidpartitiontablename
    FROM
        (SELECT
            pgp.parrelid                 AS sopaidparentoid,
            pgpr.parchildrelid           AS sopaidpartitionoid,
            sotd.sotdsize +
            sotd.sotdtoastsize +
            sotd.sotdadditionalsize      AS sopaidpartitiontablesize,
            COALESCE(soati.soatisize, 0) AS sopaidpartitionindexessize
        FROM pg_catalog.pg_partition pgp

        JOIN pg_partition_rule pgpr ON (pgp.oid = pgpr.paroid)

        JOIN gp_toolkit.gp_size_of_table_disk sotd
        ON (sotd.sotdoid = pgpr.parchildrelid)

        LEFT JOIN gp_toolkit.gp_size_of_all_table_indexes soati
        ON (soati.soatioid = pgpr.parchildrelid)
        ) AS sopaid

    JOIN gp_toolkit.__gp_fullname fnparent
    ON (sopaid.sopaidparentoid = fnparent.fnoid)

    JOIN gp_toolkit.__gp_fullname fnpart
    ON (sopaid.sopaidpartitionoid = fnpart.fnoid)
    ;

GRANT SELECT ON TABLE gp_toolkit.gp_size_of_partition_and_indexes_disk TO public;


--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_size_of_schema_disk
--
-- @doc:
--              Calculates user schema sizes in current database
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_size_of_schema_disk
AS
    SELECT
        un.aunnspname                             AS sosdnsp,
        COALESCE(sum(sotaid.sotaidtablesize), 0)  AS sosdschematablesize,
        COALESCE(sum(sotaid.sotaididxsize)  , 0)  AS sosdschemaidxsize
    FROM gp_toolkit.gp_size_of_table_and_indexes_disk sotaid

    JOIN gp_toolkit.__gp_fullname fn
    ON (sotaid.sotaidoid = fn.fnoid)

    RIGHT JOIN gp_toolkit.__gp_user_namespaces un
    ON (un.aunnspname = fn.fnnspname)

    GROUP BY
        un.aunnspname;

GRANT SELECT ON gp_toolkit.gp_size_of_schema_disk TO public;


--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_size_of_database
--
-- @doc:
--              Calculates user database sizes
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_size_of_database
AS
    SELECT
        datname                          AS sodddatname,
        pg_catalog.pg_database_size(oid) AS sodddatsize
    FROM
        pg_catalog.pg_database
    WHERE
        datname <> 'template0'
    AND datname <> 'template1'
    AND datname <> 'postgres';

GRANT SELECT ON TABLE gp_toolkit.gp_size_of_database TO public;

--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_pg_resqueue_status
--
-- @doc:
--              New version of pg_catalog.pg_resqueue_status that shows memory limits
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_resqueue_status
AS
    SELECT
        q.oid as queueid,
        q.rsqname as rsqname,
        t1.value::int as rsqcountlimit,
        t2.value::int as rsqcountvalue,
        t3.value::real as rsqcostlimit,
        t4.value::real as rsqcostvalue,
        t5.value::real as rsqmemorylimit,
        t6.value::real as rsqmemoryvalue,
        t7.value::int as rsqwaiters,
        t8.value::int as rsqholders
    FROM
        pg_resqueue q,
        pg_resqueue_status_kv() t1 (queueid oid, key text, value text),
        pg_resqueue_status_kv() t2 (queueid oid, key text, value text),
        pg_resqueue_status_kv() t3 (queueid oid, key text, value text),
        pg_resqueue_status_kv() t4 (queueid oid, key text, value text),
        pg_resqueue_status_kv() t5 (queueid oid, key text, value text),
        pg_resqueue_status_kv() t6 (queueid oid, key text, value text),
        pg_resqueue_status_kv() t7 (queueid oid, key text, value text),
        pg_resqueue_status_kv() t8 (queueid oid, key text, value text)
    WHERE
        q.oid = t1.queueid
        AND t1.queueid = t2.queueid
        AND t2.queueid = t3.queueid
        AND t3.queueid = t4.queueid
        AND t4.queueid = t5.queueid
        AND t5.queueid = t6.queueid
        AND t6.queueid = t7.queueid
        AND t7.queueid = t8.queueid
        AND t1.key = 'rsqcountlimit'
        AND t2.key = 'rsqcountvalue'
        AND t3.key = 'rsqcostlimit'
        AND t4.key = 'rsqcostvalue'
        AND t5.key = 'rsqmemorylimit'
        AND t6.key = 'rsqmemoryvalue'
        AND t7.key = 'rsqwaiters'
        AND t8.key = 'rsqholders'
    ;

GRANT SELECT ON gp_toolkit.gp_resqueue_status TO public;

--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_resgroup_config
--
-- @doc:
--              Resource group configuration
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_resgroup_config AS
    SELECT G.oid       AS groupid
         , G.rsgname   AS groupname
         , T1.value    AS concurrency
         , T2.value    AS cpu_rate_limit
         , T3.value    AS memory_limit
         , T4.value    AS memory_shared_quota
         , T5.value    AS memory_spill_ratio
         , CASE WHEN T6.value IS NULL THEN 'vmtracker'
                WHEN T6.value='0'     THEN 'vmtracker'
                WHEN T6.value='1'     THEN 'cgroup'
                ELSE 'unknown'
           END         AS memory_auditor
         , T7.value    AS cpuset
    FROM pg_resgroup G
         JOIN pg_resgroupcapability T1 ON G.oid = T1.resgroupid AND T1.reslimittype = 1
         JOIN pg_resgroupcapability T2 ON G.oid = T2.resgroupid AND T2.reslimittype = 2
         JOIN pg_resgroupcapability T3 ON G.oid = T3.resgroupid AND T3.reslimittype = 3
         JOIN pg_resgroupcapability T4 ON G.oid = T4.resgroupid AND T4.reslimittype = 4
         JOIN pg_resgroupcapability T5 ON G.oid = T5.resgroupid AND T5.reslimittype = 5
    LEFT JOIN pg_resgroupcapability T6 ON G.oid = T6.resgroupid AND T6.reslimittype = 6
    LEFT JOIN pg_resgroupcapability T7 ON G.oid = T7.resgroupid AND T7.reslimittype = 7
    ;

GRANT SELECT ON gp_toolkit.gp_resgroup_config TO public;

--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_resgroup_status
--
-- @doc:
--              Resource group runtime status information
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_resgroup_status AS
    SELECT r.rsgname, s.*
    FROM pg_resgroup_get_status(null) AS s,
         pg_resgroup AS r
    WHERE s.groupid = r.oid;

GRANT SELECT ON gp_toolkit.gp_resgroup_status TO public;

--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_resgroup_status_per_host
--
-- @doc:
--              Resource group runtime status information grouped by host
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_resgroup_status_per_host AS
    WITH s AS (
        SELECT
            rsgname
          , groupid
          , (json_each(cpu_usage)).key::smallint AS segment_id
          , (json_each(cpu_usage)).value AS cpu
          , (json_each(memory_usage)).value AS memory
        FROM gp_toolkit.gp_resgroup_status
    )
    SELECT
        s.rsgname
      , s.groupid
      , c.hostname
      , sum((s.cpu                       )::text::numeric) AS cpu
      , sum((s.memory->'used'            )::text::integer) AS memory_used
      , sum((s.memory->'available'       )::text::integer) AS memory_available
      , sum((s.memory->'quota_used'      )::text::integer) AS memory_quota_used
      , sum((s.memory->'quota_available' )::text::integer) AS memory_quota_available
      , sum((s.memory->'shared_used'     )::text::integer) AS memory_shared_used
      , sum((s.memory->'shared_available')::text::integer) AS memory_shared_available
    FROM s
    INNER JOIN pg_catalog.gp_segment_configuration AS c
        ON s.segment_id = c.content
        AND c.role = 'p'
    GROUP BY
        s.rsgname
      , s.groupid
      , c.hostname
    ;

GRANT SELECT ON gp_toolkit.gp_resgroup_status_per_host TO public;

--------------------------------------------------------------------------------
-- @view:
--              gp_toolkit.gp_resgroup_status_per_segment
--
-- @doc:
--              Resource group runtime status information grouped by segment
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_resgroup_status_per_segment AS
    WITH s AS (
        SELECT
            rsgname
          , groupid
          , (json_each(cpu_usage)).key::smallint AS segment_id
          , (json_each(cpu_usage)).value AS cpu
          , (json_each(memory_usage)).value AS memory
        FROM gp_toolkit.gp_resgroup_status
    )
    SELECT
        s.rsgname
      , s.groupid
      , c.hostname
      , s.segment_id
      , sum((s.cpu                       )::text::numeric) AS cpu
      , sum((s.memory->'used'            )::text::integer) AS memory_used
      , sum((s.memory->'available'       )::text::integer) AS memory_available
      , sum((s.memory->'quota_used'      )::text::integer) AS memory_quota_used
      , sum((s.memory->'quota_available' )::text::integer) AS memory_quota_available
      , sum((s.memory->'shared_used'     )::text::integer) AS memory_shared_used
      , sum((s.memory->'shared_available')::text::integer) AS memory_shared_available
    FROM s
    INNER JOIN pg_catalog.gp_segment_configuration AS c
        ON s.segment_id = c.content
        AND c.role = 'p'
    GROUP BY
        s.rsgname
      , s.groupid
      , c.hostname
      , s.segment_id
    ;

GRANT SELECT ON gp_toolkit.gp_resgroup_status_per_segment TO public;

--------------------------------------------------------------------------------
-- AO/CO diagnostics functions
--------------------------------------------------------------------------------

CREATE FUNCTION gp_toolkit.__gp_aoseg_history(regclass)
RETURNS TABLE(segment_id integer,
    gp_tid tid,
    gp_xmin integer,
    gp_xmin_status text,
    gp_xmin_commit_distrib_id text,
    gp_xmax integer,
    gp_xmax_status text,
    gp_xmax_commit_distrib_id text,
    gp_command_id integer,
    gp_infomask text,
    gp_update_tid tid,
    gp_visibility text,
    segno integer,
    tupcount bigint,
    eof bigint,
    eof_uncompressed bigint,
    modcount bigint,
    formatversion smallint,
    state smallint)
AS '$libdir/gp_ao_co_diagnostics', 'gp_aoseg_history_wrapper'
LANGUAGE C STRICT EXECUTE ON ALL SEGMENTS;
GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_aoseg_history(regclass) TO public;

CREATE FUNCTION gp_toolkit.__gp_aocsseg(regclass)
RETURNS TABLE(segment_id integer,
    gp_tid tid,
    segno integer,
    column_num smallint,
    physical_segno integer,
    tupcount bigint,
    eof bigint,
    eof_uncompressed bigint,
    modcount bigint,
    formatversion smallint,
    state smallint)
AS '$libdir/gp_ao_co_diagnostics', 'gp_aocsseg_wrapper'
LANGUAGE C STRICT EXECUTE ON ALL SEGMENTS;
GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_aocsseg(regclass) TO public;

CREATE FUNCTION gp_toolkit.__gp_aocsseg_history(regclass)
RETURNS TABLE(segment_id integer,
    gp_tid tid,
    gp_xmin integer,
    gp_xmin_status text,
    gp_xmin_distrib_id text,
    gp_xmax integer,
    gp_xmax_status text,
    gp_xmax_distrib_id text,
    gp_command_id integer,
    gp_infomask text,
    gp_update_tid tid,
    gp_visibility text,
    segno integer,
    column_num smallint,
    physical_segno integer,
    tupcount bigint,
    eof bigint,
    eof_uncompressed bigint,
    modcount bigint,
    formatversion smallint,
    state smallint)
AS '$libdir/gp_ao_co_diagnostics' , 'gp_aocsseg_history_wrapper'
LANGUAGE C STRICT EXECUTE ON ALL SEGMENTS;
GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_aocsseg_history(regclass) TO public;

CREATE FUNCTION gp_toolkit.__gp_aovisimap(regclass)
RETURNS TABLE (tid tid,
    segno int,
    row_num bigint)
AS '$libdir/gp_ao_co_diagnostics', 'gp_aovisimap_wrapper'
LANGUAGE C IMMUTABLE;
GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_aovisimap(regclass) TO public;

CREATE FUNCTION gp_toolkit.__gp_aovisimap_hidden_info(regclass)
RETURNS TABLE (segno integer,
    hidden_tupcount bigint,
    total_tupcount bigint)
AS '$libdir/gp_ao_co_diagnostics', 'gp_aovisimap_hidden_info_wrapper'
LANGUAGE C STRICT EXECUTE ON ALL SEGMENTS;
GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_aovisimap_hidden_info(regclass) TO public;

CREATE FUNCTION gp_toolkit.__gp_aovisimap_entry(regclass)
RETURNS TABLE(segno integer,
    first_row_num bigint,
    hidden_tupcount int,
    bitmap text)
AS '$libdir/gp_ao_co_diagnostics','gp_aovisimap_entry_wrapper'
LANGUAGE C STRICT;
GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_aovisimap_entry(regclass) TO public;

CREATE FUNCTION gp_toolkit.__gp_aoseg(regclass)
RETURNS TABLE (segment_id integer,
    segno integer, eof bigint,
    tupcount bigint,
    varblockcount bigint,
    eof_uncompressed bigint,
    modcount bigint,
    formatversion smallint,
    state smallint)
AS '$libdir/gp_ao_co_diagnostics', 'gp_aoseg_wrapper'
LANGUAGE C STRICT EXECUTE ON ALL SEGMENTS;
GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_aoseg(regclass) TO public;

CREATE TYPE gp_toolkit.__gp_aovisimap_hidden_t AS (seg int, hidden bigint, total bigint);
CREATE FUNCTION gp_toolkit.__gp_aovisimap_hidden_typed(oid)
    RETURNS SETOF gp_toolkit.__gp_aovisimap_hidden_t AS $$
    SELECT * FROM gp_toolkit.__gp_aovisimap_hidden_info($1);
$$ LANGUAGE SQL;

CREATE FUNCTION gp_toolkit.__gp_aovisimap_compaction_info(ao_oid oid,
    OUT content int, OUT datafile int, OUT compaction_possible boolean,
    OUT hidden_tupcount bigint, OUT total_tupcount bigint, OUT percent_hidden numeric)
    RETURNS SETOF RECORD AS $$
DECLARE
    hinfo_row RECORD;
    threshold float;
BEGIN
    EXECUTE 'show gp_appendonly_compaction_threshold' INTO threshold;
    FOR hinfo_row IN SELECT gp_segment_id,
    gp_toolkit.__gp_aovisimap_hidden_typed(ao_oid)::gp_toolkit.__gp_aovisimap_hidden_t
    FROM gp_dist_random('gp_id') LOOP
        content := hinfo_row.gp_segment_id;
        datafile := (hinfo_row.__gp_aovisimap_hidden_typed).seg;
        hidden_tupcount := (hinfo_row.__gp_aovisimap_hidden_typed).hidden;
        total_tupcount := (hinfo_row.__gp_aovisimap_hidden_typed).total;
        compaction_possible := false;
        IF total_tupcount > 0 THEN
            percent_hidden := (100 * hidden_tupcount / total_tupcount::numeric)::numeric(5,2);
        ELSE
            percent_hidden := 0::numeric(5,2);
        END IF;
        IF percent_hidden > threshold THEN
            compaction_possible := true;
        END IF;
        RETURN NEXT;
    END LOOP;
    RAISE NOTICE 'gp_appendonly_compaction_threshold = %', threshold;
    RETURN;
END;
$$
LANGUAGE plpgsql;

-- Workfile views
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.__gp_workfile_entries_f
--
-- @in:
--
-- @out:
--        int - segment id
--        text - path to workfile set,
--        bigint - size in bytes,
--        text - type of the spilling operation,
--        int - containing slice,
--        int - sessionid,
--        int - command_cnt,
--        int - number of files
--
-- @doc:
--        UDF to retrieve workfile sets currently present on disk on one segment
--
--------------------------------------------------------------------------------

CREATE FUNCTION gp_toolkit.__gp_workfile_entries_f_on_master()
RETURNS SETOF record
AS '$libdir/gp_workfile_mgr', 'gp_workfile_mgr_cache_entries'
LANGUAGE C VOLATILE EXECUTE ON MASTER;

GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_workfile_entries_f_on_master() TO public;

CREATE FUNCTION gp_toolkit.__gp_workfile_entries_f_on_segments()
RETURNS SETOF record
AS '$libdir/gp_workfile_mgr', 'gp_workfile_mgr_cache_entries'
LANGUAGE C VOLATILE EXECUTE ON ALL SEGMENTS;

GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_workfile_entries_f_on_segments() TO public;


--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_workfile_entries
--
-- @doc:
--        List of all the workfile sets currently present on disk
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_workfile_entries AS
WITH all_entries AS (
   SELECT C.*
          FROM gp_toolkit.__gp_workfile_entries_f_on_master() AS C (
            segid int,
            prefix text,
            size bigint,
            optype text,
            slice int,
            sessionid int,
            commandid int,
            numfiles int
          )
    UNION ALL
    SELECT C.*
          FROM gp_toolkit.__gp_workfile_entries_f_on_segments() AS C (
            segid int,
            prefix text,
            size bigint,
            optype text,
            slice int,
            sessionid int,
            commandid int,
            numfiles int
          ))
SELECT S.datname,
       S.pid,
       C.sessionid as sess_id,
       C.commandid as command_cnt,
       S.usename,
       S.query,
       C.segid,
       C.slice,
       C.optype,
       C.size,
       C.numfiles,
       C.prefix
FROM all_entries C LEFT OUTER JOIN
pg_stat_activity as S
ON C.sessionid = S.sess_id;

GRANT SELECT ON gp_toolkit.gp_workfile_entries TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_workfile_usage_per_segment
--
-- @doc:
--        Amount of disk space used for workfiles at each segment
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_workfile_usage_per_segment AS
SELECT gpseg.content AS segid, COALESCE(SUM(wfe.size),0) AS size,
    SUM(wfe.numfiles) AS numfiles
FROM (
    SELECT content
    FROM gp_segment_configuration
    WHERE role = 'p') gpseg
LEFT JOIN gp_toolkit.gp_workfile_entries wfe
ON (gpseg.content = wfe.segid)
GROUP BY gpseg.content;

GRANT SELECT ON gp_toolkit.gp_workfile_usage_per_segment TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_workfile_usage_per_query
--
-- @doc:
--        Amount of disk space used for workfiles by each query
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_workfile_usage_per_query AS
SELECT datname, pid, sess_id, command_cnt, usename, query, segid,
    SUM(size) AS size, SUM(numfiles) AS numfiles
FROM gp_toolkit.gp_workfile_entries
GROUP BY datname, pid, sess_id, command_cnt, usename, query, segid;

GRANT SELECT ON gp_toolkit.gp_workfile_usage_per_query TO public;

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.__gp_workfile_mgr_used_diskspace_f
--
-- @in:
--
-- @out:
--        int - segment id
--        bigint - size in bytes,
--
-- @doc:
--        UDF to retrieve workfile used diskspace counter in bytes per segment
--
--------------------------------------------------------------------------------

CREATE FUNCTION gp_toolkit.__gp_workfile_mgr_used_diskspace_f_on_master()
RETURNS SETOF record
AS '$libdir/gp_workfile_mgr', 'gp_workfile_mgr_used_diskspace'
LANGUAGE C VOLATILE EXECUTE ON MASTER;

GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_workfile_mgr_used_diskspace_f_on_master() TO public;

CREATE FUNCTION gp_toolkit.__gp_workfile_mgr_used_diskspace_f_on_segments()
RETURNS SETOF record
AS '$libdir/gp_workfile_mgr', 'gp_workfile_mgr_used_diskspace'
LANGUAGE C VOLATILE EXECUTE ON ALL SEGMENTS;

GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_workfile_mgr_used_diskspace_f_on_segments() TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_workfile_mgr_used_diskspace
--
-- @doc:
--        Workfile used diskspace counter in bytes per segment
--
--------------------------------------------------------------------------------
CREATE VIEW gp_toolkit.gp_workfile_mgr_used_diskspace AS
  SELECT C.*
	FROM gp_toolkit.__gp_workfile_mgr_used_diskspace_f_on_master() as C (
	  segid int,
	  bytes bigint
	)
  UNION ALL
  SELECT C.*
	FROM gp_toolkit.__gp_workfile_mgr_used_diskspace_f_on_segments() as C (
	  segid int,
	  bytes bigint
	)
ORDER BY segid;

GRANT SELECT ON gp_toolkit.gp_workfile_mgr_used_diskspace TO public;

--------------------------------------------------------------------------------

-- Finalize install
COMMIT;


-- EOF
