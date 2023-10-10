/* gpcontrib/gp_toolkit/gp_toolkit--1.1--1.2.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION gp_toolkit UPDATE TO '1.2'" to load this file. \quit

-- Return the list of existing files in the database.
-- Compared to the previous version, also show relative path of each file.
CREATE OR REPLACE VIEW gp_toolkit.__get_exist_files AS
WITH Tablespaces AS (
-- 1. The default tablespace
    SELECT 0 AS tablespace, 'base/' || d.oid::text AS dirname
    FROM pg_database d
    WHERE d.datname = current_database()
    UNION
-- 2. The global tablespace
    SELECT 1664 AS tablespace, 'global/' AS dirname
    UNION
-- 3. The user-defined tablespaces
    SELECT ts.oid AS tablespace,
       'pg_tblspc/' || ts.oid::text || '/' || get_tablespace_version_directory_name() || '/' ||
         (SELECT d.oid::text FROM pg_database d WHERE d.datname = current_database()) AS dirname
    FROM pg_tablespace ts
    WHERE ts.oid > 1664
)
SELECT tablespace, files.filename, dirname || '/' || files.filename AS filepath
FROM Tablespaces, pg_ls_dir(dirname) AS files(filename);

-- Check orphaned data files on default and user tablespaces.
-- Compared to the previous version, also show relative path of each file.
CREATE OR REPLACE VIEW gp_toolkit.__check_orphaned_files AS
SELECT f1.tablespace, f1.filename, f1.filepath
from gp_toolkit.__get_exist_files f1
LEFT JOIN gp_toolkit.__get_expect_files f2
ON f1.tablespace = f2.tablespace AND substring(f1.filename from '[0-9]+') = f2.filename
WHERE f2.tablespace IS NULL
  AND f1.filename SIMILAR TO '[0-9]+(\.)?(\_)?%';

-- New function to check orphaned files
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
        SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, *
        FROM gp_dist_random('gp_toolkit.__check_orphaned_files')
        UNION ALL
        SELECT -1 AS gp_segment_id, *
        FROM gp_toolkit.__check_orphaned_files;
    EXCEPTION
        WHEN lock_not_available THEN
            RAISE EXCEPTION 'cannot obtain SHARE lock on pg_class';
        WHEN OTHERS THEN
            RAISE;
    END;
 
    RETURN;
END;
$$;
GRANT EXECUTE ON FUNCTION gp_toolkit.__gp_check_orphaned_files_func TO public;

CREATE OR REPLACE VIEW gp_toolkit.gp_check_orphaned_files AS 
SELECT * FROM gp_toolkit.__gp_check_orphaned_files_func();

