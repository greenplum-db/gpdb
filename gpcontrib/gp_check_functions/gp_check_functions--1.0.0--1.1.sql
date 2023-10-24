/* gpcontrib/gp_check_functions/gp_check_functions--1.0.0--1.1.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION gp_check_functions UPDATE TO '1.1'" to load this file. \quit

-- Check orphaned data files on default and user tablespaces.
-- Compared to the previous version, add gp_segment_id to show which segment it is being executed.
CREATE OR REPLACE VIEW __check_orphaned_files AS
SELECT f1.tablespace, f1.filename, f1.filepath, pg_catalog.gp_execution_segment() AS gp_segment_id
from __get_exist_files f1
LEFT JOIN __get_expect_files f2
ON f1.tablespace = f2.tablespace AND substring(f1.filename from '[0-9]+') = f2.filename
WHERE f2.tablespace IS NULL
  AND f1.filename SIMILAR TO '[0-9]+(\.)?(\_)?%';

-- Function to check orphaned files.
-- Compared to the previous version, adjust the SELECT ... FROM __check_orphaned_files since we added new column to it.
-- NOTE: this function does the same lock and checks as gp_check_functions.gp_move_orphaned_files(), and it needs to be that way. 
CREATE OR REPLACE FUNCTION __gp_check_orphaned_files_func()
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
            FROM (SELECT * from pg_stat_activity UNION ALL SELECT * FROM gp_dist_random('pg_stat_activity'))q
            WHERE
            sess_id <> -1
            AND sess_id <> current_setting('gp_session_id')::int -- Exclude the current session
        ) THEN
            RAISE EXCEPTION 'There is a client session running on one or more segment. Aborting...';
        END IF;

        -- force checkpoint to make sure we do not include files that are normally pending delete
        CHECKPOINT;

        RETURN QUERY 
        SELECT v.gp_segment_id, v.tablespace, v.filename, v.filepath
        FROM gp_dist_random('__check_orphaned_files') v
        UNION ALL
        SELECT -1 AS gp_segment_id, v.tablespace, v.filename, v.filepath
        FROM __check_orphaned_files v;
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
-- NOTE: this function does the same lock and checks as gp_move_orphaned_files(),
-- and it needs to be that way. 
CREATE OR REPLACE FUNCTION __gp_check_orphaned_files_func()
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
            FROM (SELECT * from pg_stat_activity UNION ALL SELECT * FROM gp_dist_random('pg_stat_activity'))q
            WHERE
            sess_id <> -1
            AND sess_id <> current_setting('gp_session_id')::int -- Exclude the current session
        ) THEN
            RAISE EXCEPTION 'There is a client session running on one or more segment. Aborting...';
        END IF;

        -- force checkpoint to make sure we do not include files that are normally pending delete
        CHECKPOINT;

        RETURN QUERY 
        SELECT v.gp_segment_id, v.tablespace, v.filename, v.filepath
        FROM gp_dist_random('__check_orphaned_files') v
        UNION ALL
        SELECT -1 AS gp_segment_id, v.tablespace, v.filename, v.filepath
        FROM __check_orphaned_files v;
    EXCEPTION
        WHEN lock_not_available THEN
            RAISE EXCEPTION 'cannot obtain SHARE lock on pg_class';
        WHEN OTHERS THEN
            RAISE;
    END;

    RETURN;
END;
$$;

GRANT EXECUTE ON FUNCTION __gp_check_orphaned_files_func() TO public;

-- UDF to move orphaned files to a designated location
-- NOTE: this function does the same lock and checks as __gp_check_orphaned_files_func(),
-- and it needs to be that way. 
CREATE FUNCTION gp_move_orphaned_files(target_location TEXT) RETURNS TABLE (
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
        FROM (SELECT * from pg_stat_activity UNION ALL SELECT * FROM gp_dist_random('pg_stat_activity'))q
        WHERE
        sess_id <> -1
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
            FROM __check_orphaned_files o, pg_settings s
            WHERE s.name = 'data_directory'
            UNION ALL
            -- Segments
            SELECT
                 o.gp_segment_id,
                 s.setting || '/' || o.filepath as oldpath,
                 target_location || '/seg' || o.gp_segment_id::text || '_' || REPLACE(o.filepath, '/', '_') as newpath
            FROM gp_dist_random('__check_orphaned_files') o 
            JOIN (SELECT gp_execution_segment() as gp_segment_id, * FROM gp_dist_random('pg_settings')) s on o.gp_segment_id = s.gp_segment_id
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

