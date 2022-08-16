/* gpcontrib/gp_pitr/gp_pitr--1.1.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_pitr" to load this file. \quit

CREATE FUNCTION gp_create_restore_point(restore_point_name text
    --,OUT segment_id smallint, OUT restore_lsn pg_lsn -- columns left unnamed for compatibility
)
    RETURNS SETOF record
AS '$libdir/gp_pitr', 'gp_create_restore_point'
LANGUAGE C IMMUTABLE STRICT;

-- pg_switch_xlog wrapper functions to switch WAL segment files on Greenplum cluster-wide
CREATE FUNCTION gp_switch_wal_on_all_segments (OUT gp_segment_id int, OUT pg_switch_wal pg_lsn)
    RETURNS SETOF RECORD AS
$$
DECLARE
seg_id int;
BEGIN
EXECUTE 'SELECT pg_catalog.gp_execution_segment()' INTO seg_id;
-- check if execute in entrydb QE to prevent giving wrong results
IF seg_id = -1 THEN
    RAISE EXCEPTION 'Cannot execute in entrydb, this query is not currently supported by GPDB.';
END IF;
RETURN QUERY SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, * FROM pg_catalog.pg_switch_xlog();
END;
$$ LANGUAGE plpgsql EXECUTE ON ALL SEGMENTS;

CREATE FUNCTION gp_switch_wal (OUT gp_segment_id int, OUT pg_switch_wal pg_lsn)
    RETURNS SETOF RECORD
AS
  'SELECT * FROM @extschema@.gp_switch_wal_on_all_segments()
   UNION ALL
   SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, * FROM pg_catalog.pg_switch_xlog()'
LANGUAGE SQL EXECUTE ON MASTER;

COMMENT ON FUNCTION gp_switch_wal_on_all_segments() IS 'Switch WAL segment files on all primary segments';
COMMENT ON FUNCTION gp_switch_wal() IS 'Switch WAL segment files on all segments';

REVOKE EXECUTE ON FUNCTION gp_switch_wal_on_all_segments() FROM public;
REVOKE EXECUTE ON FUNCTION gp_switch_wal() FROM public;

CREATE OR REPLACE VIEW gp_stat_archiver AS
SELECT -1 AS gp_segment_id, * FROM pg_stat_archiver
UNION
SELECT gp_execution_segment() AS gp_segment_id, * FROM gp_dist_random('pg_stat_archiver');

GRANT SELECT ON gp_stat_archiver TO PUBLIC;
