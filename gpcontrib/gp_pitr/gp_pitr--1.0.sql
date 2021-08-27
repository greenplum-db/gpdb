\echo Use "CREATE EXTENSION gp_pitr" to load this file. \quit

CREATE FUNCTION gp_create_restore_point(restore_point_name text
                    --,OUT segment_id smallint, OUT restore_lsn pg_lsn -- columns left unnamed for compatibility
                    )
RETURNS SETOF record
AS '$libdir/gp_pitr', 'gp_create_restore_point'
LANGUAGE C IMMUTABLE STRICT;
