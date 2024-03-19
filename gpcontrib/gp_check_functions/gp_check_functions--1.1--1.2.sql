-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION gp_check_functions UPDATE TO '1.2" to load this file. \quit

--------------------------------------------------------------------------------
-- @function:
--        __gp_check_orphaned_files_func
--
-- @in:
--
-- @out:
--        gp_segment_id int - segment content ID
--        tablespace oid - tablespace OID
--        filename text - name of the orphaned file
--        filepath text - relative path of the orphaned file in data directory
--
-- @doc:
--        (Internal UDF, shouldn't be exposed)
--        UDF to retrieve orphaned files and their paths
--
--------------------------------------------------------------------------------

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
            AND state <> 'idle' -- Exclude idle session like GDD
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

--------------------------------------------------------------------------------
-- @function:
--        gp_move_orphaned_files
--
-- @in:
--        target_location text - directory where we move the orphaned files to
--
-- @out:
--        gp_segment_id int - segment content ID
--        move_success bool - whether the move attempt succeeded
--        oldpath text - filepath (name included) of the orphaned file before moving
--        newpath text - filepath (name included) of the orphaned file after moving
--
-- @doc:
--        UDF to move orphaned files to a designated location
--
--------------------------------------------------------------------------------

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
        AND state <> 'idle' -- Exclude idle session like GDD
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
        SELECT s1.gp_segment_id, s1.oldpath, s1.newpath, pg_file_rename(s1.oldpath, s1.newpath, NULL) AS move_success
        FROM
        (
            SELECT
                o.gp_segment_id,
                s.setting || '/' || o.filepath as oldpath,
                target_location || '/seg' || o.gp_segment_id::text || '_' || REPLACE(o.filepath, '/', '_') as newpath
            FROM __check_orphaned_files o, pg_settings s
            WHERE s.name = 'data_directory'
        ) s1
        UNION ALL
        -- Segments
        SELECT s2.gp_segment_id, s2.oldpath, s2.newpath, pg_file_rename(s2.oldpath, s2.newpath, NULL) AS move_success
        FROM
        (
            SELECT
                o.gp_segment_id,
                s.setting || '/' || o.filepath as oldpath,
                target_location || '/seg' || o.gp_segment_id::text || '_' || REPLACE(o.filepath, '/', '_') as newpath
            FROM gp_dist_random('__check_orphaned_files') o
            JOIN (SELECT gp_execution_segment() as gp_segment_id, * FROM gp_dist_random('pg_settings')) s on o.gp_segment_id = s.gp_segment_id
            WHERE s.name = 'data_directory'
        ) s2
    ) q
    ORDER BY q.gp_segment_id, q.oldpath;
EXCEPTION
    WHEN lock_not_available THEN
        RAISE EXCEPTION 'cannot obtain SHARE lock on pg_class';
    WHEN OTHERS THEN
        RAISE;
END;
$$;
