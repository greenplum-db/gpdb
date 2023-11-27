/* gpcontrib/gp_toolkit/gp_toolkit--1.3--1.4.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION gp_toolkit UPDATE TO '1.4'" to load this file. \quit

-- Check orphaned data files on default and user tablespaces.
-- Compared to the previous version, add gp_segment_id to show which segment it is being executed.
CREATE OR REPLACE VIEW gp_toolkit.__check_orphaned_files AS
SELECT f1.tablespace, f1.filename, f1.filepath, pg_catalog.gp_execution_segment() AS gp_segment_id
from gp_toolkit.__get_exist_files f1
LEFT JOIN gp_toolkit.__get_expect_files f2
ON f1.tablespace = f2.tablespace AND substring(f1.filename from '[0-9]+') = f2.filename
WHERE f2.tablespace IS NULL
  AND f1.filename SIMILAR TO '[0-9]+(\.)?(\_)?%';

-- Function to check orphaned files.
-- Compared to the previous version, adjust the SELECT ... FROM __check_orphaned_files since we added new column to it.
-- NOTE: this function does the same lock and checks as gp_toolkit.gp_move_orphaned_files(), and it needs to be that way. 
CREATE OR REPLACE FUNCTION gp_toolkit.__gp_check_orphaned_files_func()
RETURNS TABLE (
    gp_segment_id int,
    tablespace oid,
    filename text,
    filepath text
)
LANGUAGE plpgsql AS $$
BEGIN
    BEGIN
        -- lock pg_class so that no one will be adding/altering relfilenodes
        LOCK TABLE pg_class IN SHARE MODE NOWAIT;
       
        -- make sure no other active/idle transaction is running
        IF EXISTS (
            SELECT 1
            FROM gp_stat_activity
            WHERE
            sess_id <> -1 AND backend_type IN ('client backend', 'unknown process type') -- exclude background worker types
            AND sess_id <> current_setting('gp_session_id')::int -- Exclude the current session
        ) THEN
            RAISE EXCEPTION 'There is a client session running on one or more segment. Aborting...';
        END IF;
       
        -- force checkpoint to make sure we do not include files that are normally pending delete
        CHECKPOINT;
       
        RETURN QUERY 
        SELECT v.gp_segment_id, v.tablespace, v.filename, v.filepath
        FROM gp_dist_random('gp_toolkit.__check_orphaned_files') v
        UNION ALL
        SELECT -1 AS gp_segment_id, v.tablespace, v.filename, v.filepath
        FROM gp_toolkit.__check_orphaned_files v;
    EXCEPTION
        WHEN lock_not_available THEN
            RAISE EXCEPTION 'cannot obtain SHARE lock on pg_class';
        WHEN OTHERS THEN
            RAISE;
    END;
 
    RETURN;
END;
$$;

-- Function to move orphaned files to a designated location.
-- NOTE: this function does the same lock and checks as gp_toolkit.__gp_check_orphaned_files_func(), and it needs to be that way. 
CREATE FUNCTION gp_toolkit.gp_move_orphaned_files(target_location TEXT) RETURNS TABLE (
    gp_segment_id INT,
    move_success BOOL,
    oldpath TEXT,
    newpath TEXT
)
LANGUAGE plpgsql AS $$
BEGIN
    -- lock pg_class so that no one will be adding/altering relfilenodes
    LOCK TABLE pg_class IN SHARE MODE NOWAIT;
    
    -- make sure no other active/idle transaction is running
    IF EXISTS (
        SELECT 1
        FROM gp_stat_activity
        WHERE
        sess_id <> -1 AND backend_type IN ('client backend', 'unknown process type') -- exclude background worker types
        AND sess_id <> current_setting('gp_session_id')::int -- Exclude the current session
    ) THEN
        RAISE EXCEPTION 'There is a client session running on one or more segment. Aborting...';
    END IF;
    
    -- force checkpoint to make sure we do not include files that are normally pending delete
    CHECKPOINT;
       
    RETURN QUERY
    SELECT 
        q.gp_segment_id,
        q.move_success,
        q.oldpath,
        q.newpath
    FROM (
        WITH OrphanedFiles AS (
            -- Coordinator
            SELECT 
                o.gp_segment_id,
                s.setting || '/' || o.filepath as oldpath,
                target_location || '/seg' || o.gp_segment_id::text || '_' || REPLACE(o.filepath, '/', '_') as newpath
            FROM gp_toolkit.__check_orphaned_files o 
            JOIN gp_settings s on o.gp_segment_id = s.gp_segment_id 
            WHERE s.name = 'data_directory'
            UNION ALL
            -- Segments
            SELECT
                 o.gp_segment_id,
                 s.setting || '/' || o.filepath as oldpath,
                 target_location || '/seg' || o.gp_segment_id::text || '_' || REPLACE(o.filepath, '/', '_') as newpath
            FROM gp_dist_random('gp_toolkit.__check_orphaned_files') o 
            JOIN gp_settings s on o.gp_segment_id = s.gp_segment_id
            WHERE s.name = 'data_directory'
        )
        SELECT 
            OrphanedFiles.gp_segment_id,
            OrphanedFiles.oldpath,
            OrphanedFiles.newpath,
            pg_file_rename(OrphanedFiles.oldpath, OrphanedFiles.newpath, NULL) AS move_success
        FROM OrphanedFiles
    ) q ORDER BY q.gp_segment_id, q.oldpath;
EXCEPTION
    WHEN lock_not_available THEN
        RAISE EXCEPTION 'cannot obtain SHARE lock on pg_class';
    WHEN OTHERS THEN
        RAISE;
END;
$$;

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.pg_partition_rank
-- @in:
--        oid - oid of table for which to determine range partition rank
-- @out:
--        int - range partition rank
--
-- @doc:
--        Get range partition child rank for given table
--
--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION gp_toolkit.pg_partition_rank(rp regclass)
    RETURNS int
AS 'gp_toolkit.so', 'pg_partition_rank'
    LANGUAGE C VOLATILE STRICT NO SQL;

GRANT EXECUTE ON FUNCTION gp_toolkit.pg_partition_rank(regclass) TO public;

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.pg_partition_bound_value
-- @in:
--        oid - oid of the partition to get the bound value for
--        text - bound type: 'from', 'to' for a range partition, or 'in' for a list partition
-- @out:
--        text - bound value
--
-- @doc:
--        Get the partition bound value for a given partitioned table
--
--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION gp_toolkit.pg_partition_bound_value(partrel regclass, bound_type text)
    RETURNS text AS $$
DECLARE
v_relpartbound text;
    v_bound_value text;
    v_parent_table regclass;
    v_nkeys int;
BEGIN
    -- Check if the given table is a non-default child partition
SELECT inhparent INTO v_parent_table
FROM pg_inherits
WHERE inhrelid = partrel;

IF v_parent_table IS NULL THEN
        RETURN NULL;
END IF;

-- Check if the parent table is partitioned by a single key
SELECT partnatts INTO v_nkeys
FROM pg_partitioned_table
WHERE partrelid = v_parent_table;

IF v_nkeys IS NOT NULL AND v_nkeys != 1 THEN
        RETURN NULL;
END IF;

-- Get the partition bounds
SELECT pg_get_expr(relpartbound, oid) INTO v_relpartbound
FROM pg_class
WHERE oid = partrel;

-- Parse the bound value from relpartbound
IF lower(bound_type) = 'from' THEN
SELECT (regexp_matches(v_relpartbound, 'FOR VALUES FROM \((.+)\) TO \((.+)\)'))[1] INTO v_bound_value;
ELSIF lower(bound_type) = 'to' THEN
SELECT (regexp_matches(v_relpartbound, 'FOR VALUES FROM \((.+)\) TO \((.+)\)'))[2] INTO v_bound_value;
ELSIF lower(bound_type) = 'in' THEN
SELECT (regexp_matches(v_relpartbound, 'FOR VALUES IN \((.+)\)'))[1] INTO v_bound_value;
ELSE
        RAISE EXCEPTION 'Invalid bound type: %', bound_type;
END IF;

RETURN v_bound_value;
END;
$$ LANGUAGE plpgsql;

GRANT EXECUTE ON FUNCTION gp_toolkit.pg_partition_bound_value(regclass,text) TO public;

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.pg_partition_range_from
-- @in:
--        oid - oid of the range partition to get the lower bound value for
-- @out:
--        text - bound value
--
-- @doc:
--        Get the lower bound value for a given range partition child.
--        wrapper around pg_partition_bound_value()
--
--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION gp_toolkit.pg_partition_range_from(rp regclass)
    RETURNS text AS $$
SELECT gp_toolkit.pg_partition_bound_value(rp, 'from');
$$ LANGUAGE sql;

GRANT EXECUTE ON FUNCTION gp_toolkit.pg_partition_range_from(regclass) TO public;

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.pg_partition_range_to
-- @in:
--        oid - oid of the range partition to get the upper bound value for
-- @out:
--        text - bound value
--
-- @doc:
--        Get the upper bound value for a given range partition child.
--        wrapper around pg_partition_bound_value()
--
--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION gp_toolkit.pg_partition_range_to(rp regclass)
    RETURNS text AS $$
SELECT gp_toolkit.pg_partition_bound_value(rp, 'to');
$$ LANGUAGE sql;

GRANT EXECUTE ON FUNCTION gp_toolkit.pg_partition_range_to(regclass) TO public;

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.pg_partition_list_values
-- @in:
--        oid - oid of the list partition to get the bound value for
-- @out:
--        text - bound value
--
-- @doc:
--        Get the list of values for a given list partition child.
--        wrapper around pg_partition_bound_value()
--
--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION gp_toolkit.pg_partition_list_values(rp regclass)
    RETURNS text AS $$
SELECT gp_toolkit.pg_partition_bound_value(rp, 'in'::text);
$$ LANGUAGE sql;

GRANT EXECUTE ON FUNCTION gp_toolkit.pg_partition_list_values(regclass) TO public;

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.pg_partition_isdefault
-- @in:
--        oid - oid of the partition to check
-- @out:
--        boolean - true if the partition is the default partition
--
-- @doc:
--        Get whether a given partition is a default partition
--
--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION gp_toolkit.pg_partition_isdefault(relid regclass)
    RETURNS BOOLEAN AS $$
DECLARE
boundspec TEXT;
BEGIN
    -- Get the partition bound definition for the relation
SELECT pg_get_expr(relpartbound, oid) INTO boundspec
FROM pg_catalog.pg_class
WHERE oid = relid;

-- If partition_def is null, the relation is not a partition at all
IF boundspec IS NULL THEN
        RETURN FALSE;
END IF;

-- Check if the partition bound spec exactly matches 'DEFAULT'
RETURN boundspec = 'DEFAULT';
END;
$$ LANGUAGE plpgsql;

GRANT EXECUTE ON FUNCTION gp_toolkit.pg_partition_isdefault(regclass) TO public;

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.pg_partition_lowest_child
-- @in:
--        oid - oid of a range partitioned table
-- @out:
--        oid - oid of the lowest child partition
--
-- @doc:
--        Get the oid of the lowest child partition for a given partitioned table
--
--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION gp_toolkit.pg_partition_lowest_child(rp regclass)
    RETURNS regclass
AS 'gp_toolkit.so', 'pg_partition_lowest_child'
    LANGUAGE C VOLATILE STRICT NO SQL;

GRANT EXECUTE ON FUNCTION gp_toolkit.pg_partition_lowest_child(regclass) TO public;

--------------------------------------------------------------------------------
-- @function:
--        gp_toolkit.pg_partition_highest_child
-- @in:
--        oid - oid of a range partitioned table
-- @out:
--        oid - oid of the highest child partition
--
-- @doc:
--        Get the oid of the lowest child partition for a given partitioned table
--
--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION gp_toolkit.pg_partition_highest_child(rp regclass)
    RETURNS regclass
AS 'gp_toolkit.so', 'pg_partition_highest_child'
    LANGUAGE C VOLATILE STRICT NO SQL;

GRANT EXECUTE ON FUNCTION gp_toolkit.pg_partition_highest_child(regclass) TO public;

--------------------------------------------------------------------------------
-- Supporting cast for gp_toolkit.gp_partitions
--------------------------------------------------------------------------------
CREATE TYPE get_partition_result AS (
    relid regclass,
    parentid regclass,
    isleaf bool,
    partitionlevel int,
    partitiontype text,
    partitionrank int,
    is_default bool
    );

CREATE OR REPLACE FUNCTION gp_toolkit.gp_get_partitions(rp regclass)
    RETURNS SETOF get_partition_result
AS 'gp_toolkit.so', 'gp_get_partitions'
    LANGUAGE C VOLATILE STRICT NO SQL;

GRANT EXECUTE ON FUNCTION gp_toolkit.gp_get_partitions(regclass) TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_partitions
--
-- @doc:
--        This view provides necessary information about all the child
--        partitions. It plays similar role as the pg_partitions view in 6X and
--        has most of the same columns.
--
--        Notable differences from 6X:
--        (1) The root is level = 0, its immediate children are level 1 and so on.
--        (2) Immediate children of the root have parentpartitiontablename equal
--            to that of the root (instead of being null).
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW gp_toolkit.gp_partitions AS
WITH default_ts(default_spcname) AS
(select s.spcname
    from pg_database, pg_tablespace s
    where datname = current_database() and dattablespace = s.oid),
partitions AS
(SELECT p.*,
        pg_get_expr(pc.relpartbound, pc.oid) AS bound,
        rns.nspname AS rootnamespacename,
        pns.nspname AS partitionschemaname,
        pc.relname AS partitiontablename,
        coalesce(rt.spcname, default_spcname) AS parenttablespacename,
        coalesce(pt.spcname, default_spcname) AS partitiontablespacename
    FROM
    (SELECT relnamespace,
            relname AS roottablename,
            (gp_toolkit.gp_get_partitions(oid)).*
     FROM pg_class
            WHERE relkind = 'p'
            AND oid NOT IN (SELECT inhrelid FROM pg_inherits)) p
     JOIN pg_class pc ON p.relid = pc.oid
     JOIN pg_class parentc ON parentc.oid = p.parentid
     LEFT JOIN pg_namespace rns ON p.relnamespace = rns.oid
     LEFT JOIN pg_namespace pns ON pc.relnamespace = pns.oid
     LEFT JOIN pg_tablespace rt ON parentc.reltablespace = rt.oid
     LEFT JOIN pg_tablespace pt ON pc.reltablespace = pt.oid
     JOIN default_ts ON 1=1)

SELECT
    rootnamespacename AS schemaname,
    roottablename AS tablename,
    partitionschemaname,
    partitiontablename,
    parentid::regclass AS parentpartitiontablename,
        partitiontype,
    partitionlevel,
    partitionrank,
    CASE
        WHEN partitiontype = 'list'
            THEN substring(bound FROM 'FOR VALUES IN \((.+)\)')
        END AS partitionlistvalues,
    CASE
        WHEN partitiontype = 'range'
            THEN substring(bound FROM 'FOR VALUES FROM \((.+)\) TO \((.+)\)')
        END AS partitionrangestart,
    CASE
        WHEN partitiontype = 'range'
            THEN substring(bound FROM 'TO \((.+)\)')
        END AS partitionrangeend,
    is_default AS partitionisdefault,
    bound AS partitionboundary,
    parenttablespacename AS parenttablespace,
    partitiontablespacename AS partitiontablespace

FROM partitions;

GRANT SELECT ON gp_toolkit.gp_partitions TO public;

