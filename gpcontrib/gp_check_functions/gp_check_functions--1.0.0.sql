-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_check_functions" to load this file. \quit

CREATE OR REPLACE FUNCTION get_tablespace_version_directory_name()
RETURNS text
AS '$libdir/gp_check_functions'
LANGUAGE C;

--------------------------------------------------------------------------------
-- @function:
--        __get_ao_segno_list
--
-- @in:
--
-- @out:
--        oid - relation oid
--        int - segment number
--
-- @doc:
--        UDF to retrieve AO segment file numbers for each ao_row table
--
--------------------------------------------------------------------------------

CREATE OR REPLACE FUNCTION __get_ao_segno_list()
RETURNS TABLE (relid oid, segno int) AS
$$
DECLARE
  table_name text;
  rec record;
  cur refcursor;
  row record;
BEGIN
  -- iterate over the aoseg relations
  FOR rec IN SELECT sc.relname segrel, tc.oid tableoid 
             FROM pg_appendonly a 
             JOIN pg_class tc ON a.relid = tc.oid 
             JOIN pg_am am ON tc.relam = am.oid 
             JOIN pg_class sc ON a.segrelid = sc.oid 
             WHERE amname = 'ao_row' 
  LOOP
    table_name := rec.segrel;
    -- Fetch and return each row from the aoseg table
    BEGIN
      OPEN cur FOR EXECUTE format('SELECT segno FROM pg_aoseg.%I', table_name);
      SELECT rec.tableoid INTO relid;
      LOOP
        FETCH cur INTO row;
        EXIT WHEN NOT FOUND;
        segno := row.segno;
        IF segno <> 0 THEN -- there's no '.0' file, it means the file w/o extension
          RETURN NEXT;
        END IF;
      END LOOP;
      CLOSE cur;
    EXCEPTION
      -- If failed to open the aoseg table (e.g. the table itself is missing), continue
      WHEN OTHERS THEN
      RAISE WARNING 'Failed to read %: %', table_name, SQLERRM;
    END;
  END LOOP;
  RETURN;
END;
$$
LANGUAGE plpgsql;

GRANT EXECUTE ON FUNCTION __get_ao_segno_list() TO public;

--------------------------------------------------------------------------------
-- @function:
--        __get_aoco_segno_list
--
-- @in:
--
-- @out:
--        oid - relation oid
--        int - segment number
--
-- @doc:
--        UDF to retrieve AOCO segment file numbers for each ao_column table
--
--------------------------------------------------------------------------------

CREATE OR REPLACE FUNCTION __get_aoco_segno_list()
RETURNS TABLE (relid oid, segno int) AS
$$
DECLARE
  table_name text;
  rec record;
  cur refcursor;
  row record;
BEGIN
  -- iterate over the aocoseg relations
  FOR rec IN SELECT sc.relname segrel, tc.oid tableoid
             FROM pg_appendonly a
             JOIN pg_class tc ON a.relid = tc.oid
             JOIN pg_am am ON tc.relam = am.oid
             JOIN pg_class sc ON a.segrelid = sc.oid
             WHERE amname = 'ao_column'
  LOOP
    table_name := rec.segrel;
    -- Fetch and return each extended segno corresponding to filenum and segno in the aocoseg table
    BEGIN
      OPEN cur FOR EXECUTE format('SELECT ((a.filenum - 1) * 128 + s.segno) as segno '
                                  'FROM (SELECT * FROM pg_attribute_encoding '
                                  'WHERE attrelid = %s) a CROSS JOIN pg_aoseg.%I s', 
                                   rec.tableoid, table_name);
      SELECT rec.tableoid INTO relid;
      LOOP
        FETCH cur INTO row;
        EXIT WHEN NOT FOUND;
        segno := row.segno;
        IF segno <> 0 THEN -- there's no '.0' file, it means the file w/o extension
          RETURN NEXT;
        END IF;
      END LOOP;
      CLOSE cur;
    EXCEPTION
      -- If failed to open the aocoseg table (e.g. the table itself is missing), continue
      WHEN OTHERS THEN
      RAISE WARNING 'Failed to read %: %', table_name, SQLERRM;
    END;
  END LOOP;
  RETURN;
END;
$$
LANGUAGE plpgsql;

GRANT EXECUTE ON FUNCTION __get_aoco_segno_list() TO public;

--------------------------------------------------------------------------------
-- @view:
--        __get_exist_files
--
-- @doc:
--        Retrieve a list of all existing data files in the default
--        and user tablespaces.
--
--------------------------------------------------------------------------------
-- return the list of existing files in the database
CREATE OR REPLACE VIEW __get_exist_files AS
-- 1. List of files in the default tablespace
SELECT 0 AS tablespace, filename 
FROM pg_ls_dir('base/' || (
  SELECT d.oid::text
  FROM pg_database d
  WHERE d.datname = current_database()
))
AS filename
UNION
-- 2. List of files in the global tablespace
SELECT 1664 AS tablespace, filename
FROM pg_ls_dir('global/') 
AS filename
UNION
-- 3. List of files in user-defined tablespaces
SELECT ts.oid AS tablespace,
       pg_ls_dir('pg_tblspc/' || ts.oid::text || '/' || get_tablespace_version_directory_name() || '/' || 
         (SELECT d.oid::text FROM pg_database d WHERE d.datname = current_database()), true/*missing_ok*/,false/*include_dot*/) AS filename
FROM pg_tablespace ts
WHERE ts.oid > 1664; 

GRANT SELECT ON __get_exist_files TO public;

--------------------------------------------------------------------------------
-- @view:
--        ____get_expect_files
--
-- @doc:
--        Retrieve a list of expected data files in the database,
--        using the knowledge from catalogs. This does not include
--        any extended data files.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW __get_expect_files AS
SELECT s.reltablespace AS tablespace, s.relname, a.amname AS AM,
       (CASE WHEN s.relfilenode != 0 THEN s.relfilenode ELSE pg_relation_filenode(s.oid) END)::text AS filename
FROM pg_class s
LEFT JOIN pg_am a ON s.relam = a.oid
WHERE s.relkind != 'v' AND s.relkind != 'f';

GRANT SELECT ON __get_expect_files TO public;

--------------------------------------------------------------------------------
-- @view:
--        __get_expect_files_ext
--
-- @doc:
--        Retrieve a list of expected data files in the database,
--        using the knowledge from catalogs. This includes all
--        the extended data files for AO/CO tables.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW __get_expect_files_ext AS
SELECT s.reltablespace AS tablespace, s.relname, a.amname AS AM,
       (CASE WHEN s.relfilenode != 0 THEN s.relfilenode ELSE pg_relation_filenode(s.oid) END)::text AS filename
FROM pg_class s LEFT JOIN pg_am a ON s.relam = a.oid
WHERE s.relkind != 'v' AND s.relkind != 'f'
UNION
-- AO extended files
SELECT c.reltablespace AS tablespace, c.relname, a.amname AS AM,
       format(c.relfilenode::text || '.' || s.segno::text) AS filename
FROM __get_ao_segno_list() s
JOIN pg_class c ON s.relid = c.oid
LEFT JOIN pg_am a ON c.relam = a.oid
WHERE c.relkind != 'v' AND c.relkind != 'f' -- AO tables shouldn't be these but just in case
UNION
-- CO extended files
SELECT c.reltablespace AS tablespace, c.relname, a.amname AS AM,
       format(c.relfilenode::text || '.' || s.segno::text) AS filename
FROM __get_aoco_segno_list() s
JOIN pg_class c ON s.relid = c.oid
LEFT JOIN pg_am a ON c.relam = a.oid
WHERE c.relkind != 'v' AND c.relkind != 'f'; -- AOCO tables shouldn't be these but just in case

GRANT SELECT ON __get_expect_files_ext TO public;

--------------------------------------------------------------------------------
-- @view:
--        __check_orphaned_files
--
-- @doc:
--        Check orphaned data files on default and user tablespaces,
--        not including extended files.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW __check_orphaned_files AS
SELECT f1.tablespace, f1.filename
from __get_exist_files f1
LEFT JOIN __get_expect_files f2
ON f1.tablespace = f2.tablespace AND f1.filename = f2.filename
WHERE f2.tablespace IS NULL
  AND f1.filename SIMILAR TO '[0-9]+';

GRANT SELECT ON __check_orphaned_files TO public;

--------------------------------------------------------------------------------
-- @view:
--        __check_orphaned_files_ext
--
-- @doc:
--        Check orphaned data files on default and user tablespaces,
--        including extended files.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW __check_orphaned_files_ext AS
SELECT f1.tablespace, f1.filename
FROM __get_exist_files f1
LEFT JOIN __get_expect_files_ext f2
ON f1.tablespace = f2.tablespace AND f1.filename = f2.filename
WHERE f2.tablespace IS NULL
  AND f1.filename SIMILAR TO '[0-9]+(\.[0-9]+)?'
  AND NOT EXISTS (
    -- XXX: not supporting heap for now, do not count them
    SELECT 1 FROM pg_class c 
    JOIN pg_am a 
    ON c.relam = a.oid 
    WHERE c.relfilenode::text = split_part(f1.filename, '.', 1) 
        AND a.amname = 'heap'
  );

GRANT SELECT ON __check_orphaned_files_ext TO public;

--------------------------------------------------------------------------------
-- @view:
--        __check_missing_files
--
-- @doc:
--        Check missing data files on default and user tablespaces,
--        not including extended files.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW __check_missing_files AS
SELECT f1.tablespace, f1.relname, f1.filename
from __get_expect_files f1
LEFT JOIN __get_exist_files f2
ON f1.tablespace = f2.tablespace AND f1.filename = f2.filename
WHERE f2.tablespace IS NULL
  AND f1.filename SIMILAR TO '[0-9]+';

GRANT SELECT ON __check_missing_files TO public;

--------------------------------------------------------------------------------
-- @view:
--        __check_missing_files_ext
--
-- @doc:
--        Check missing data files on default and user tablespaces,
--        including extended files.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW __check_missing_files_ext AS
SELECT f1.tablespace, f1.relname, f1.filename
FROM __get_expect_files_ext f1
LEFT JOIN __get_exist_files f2
ON f1.tablespace = f2.tablespace AND f1.filename = f2.filename
WHERE f2.tablespace IS NULL
  AND f1.filename SIMILAR TO '[0-9]+(\.[0-9]+)?';

GRANT SELECT ON __check_missing_files_ext TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_check_orphaned_files
--
-- @doc:
--        User-facing view of __check_orphaned_files. 
--        Gather results from coordinator and all segments.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW gp_check_orphaned_files AS 
SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, *
FROM gp_dist_random('__check_orphaned_files')
UNION ALL 
SELECT -1 AS gp_segment_id, *
FROM __check_orphaned_files;

GRANT SELECT ON gp_check_orphaned_files TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_check_orphaned_files_ext
--
-- @doc:
--        User-facing view of __check_orphaned_files_ext.
--        Gather results from coordinator and all segments.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW gp_check_orphaned_files_ext AS 
SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, *
FROM gp_dist_random('__check_orphaned_files_ext')
UNION ALL 
SELECT -1 AS gp_segment_id, *
FROM __check_orphaned_files; -- not checking ext on coordinator

GRANT SELECT ON gp_check_orphaned_files_ext TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_check_missing_files
--
-- @doc:
--        User-facing view of __check_missing_files. 
--        Gather results from coordinator and all segments.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW gp_check_missing_files AS 
SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, *
FROM gp_dist_random('__check_missing_files')
UNION ALL 
SELECT -1 AS gp_segment_id, *
FROM __check_missing_files;

GRANT SELECT ON gp_check_missing_files TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_check_missing_files_ext
--
-- @doc:
--        User-facing view of __check_missing_files_ext.
--        Gather results from coordinator and all segments.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW gp_check_missing_files_ext AS 
SELECT pg_catalog.gp_execution_segment() AS gp_segment_id, *
FROM gp_dist_random('__check_missing_files_ext')
UNION ALL 
SELECT -1 AS gp_segment_id, *
FROM __check_missing_files; -- not checking ext on coordinator

GRANT SELECT ON gp_check_missing_files_ext TO public;

