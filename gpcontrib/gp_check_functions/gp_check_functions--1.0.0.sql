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
--        eof - eof of the segment file
--
-- @doc:
--        UDF to retrieve AO segment file numbers for each ao_row table
--
--------------------------------------------------------------------------------

CREATE OR REPLACE FUNCTION __get_ao_segno_list()
RETURNS TABLE (relid oid, segno int, eof bigint) AS
$$
DECLARE
  table_name text;
  rec record;
  cur refcursor;
  row record;
BEGIN
  -- iterate over the aoseg relations
  FOR rec IN SELECT tc.oid tableoid, tc.relname, ns.nspname
             FROM pg_appendonly a 
             JOIN pg_class tc ON a.relid = tc.oid 
             JOIN pg_namespace ns ON tc.relnamespace = ns.oid
             WHERE tc.relstorage = 'a' 
  LOOP
    table_name := rec.relname;
    -- Fetch and return each row from the aoseg table
    BEGIN
      OPEN cur FOR EXECUTE format('SELECT segno, eof '
                                  'FROM gp_toolkit.__gp_aoseg(''%I.%I'') ',
                                   rec.nspname, rec.relname);
      SELECT rec.tableoid INTO relid;
      LOOP
        FETCH cur INTO row;
        EXIT WHEN NOT FOUND;
        segno := row.segno;
        eof := row.eof;
        IF segno <> 0 THEN -- there's no '.0' file, it means the file w/o extension
          RETURN NEXT;
        END IF;
      END LOOP;
      CLOSE cur;
    EXCEPTION
      -- If failed to open the aoseg table (e.g. the table itself is missing), continue
      WHEN OTHERS THEN
      RAISE WARNING 'Failed to get aoseg info for %: %', table_name, SQLERRM;
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
--        eof - eof of the segment file
--
-- @doc:
--        UDF to retrieve AOCO segment file numbers for each ao_column table
--
--------------------------------------------------------------------------------

CREATE OR REPLACE FUNCTION __get_aoco_segno_list()
RETURNS TABLE (relid oid, segno int, eof bigint) AS
$$
DECLARE
  table_name text;
  rec record;
  cur refcursor;
  row record;
BEGIN
  -- iterate over the aocoseg relations
  FOR rec IN SELECT tc.oid tableoid, tc.relname, ns.nspname
             FROM pg_appendonly a
             JOIN pg_class tc ON a.relid = tc.oid
             JOIN pg_namespace ns ON tc.relnamespace = ns.oid
             WHERE tc.relstorage = 'c'
  LOOP
    table_name := rec.relname;
    -- Fetch and return each extended segno corresponding to attnum and segno in the aocoseg table
    BEGIN
      OPEN cur FOR EXECUTE format('SELECT physical_segno as segno, eof '
                                  'FROM gp_toolkit.__gp_aocsseg(''%I.%I'') ',
                                   rec.nspname, rec.relname);
      SELECT rec.tableoid INTO relid;
      LOOP
        FETCH cur INTO row;
        EXIT WHEN NOT FOUND;
        segno := row.segno;
        eof := row.eof;
        IF segno <> 0 THEN -- there's no '.0' file, it means the file w/o extension
          RETURN NEXT;
        END IF;
      END LOOP;
      CLOSE cur;
    EXCEPTION
      -- If failed to open the aocoseg table (e.g. the table itself is missing), continue
      WHEN OTHERS THEN
      RAISE WARNING 'Failed to get aocsseg info for %: %', table_name, SQLERRM;
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
--        __get_expect_files
--
-- @doc:
--        Retrieve a list of expected data files in the database,
--        using the knowledge from catalogs. This does not include
--        any extended data files, nor does it include external,
--        foreign or virtual tables.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW __get_expect_files AS
SELECT s.reltablespace AS tablespace, s.relname, s.relstorage,
       (CASE WHEN s.relfilenode != 0 THEN s.relfilenode ELSE pg_relation_filenode(s.oid) END)::text AS filename
FROM pg_class s
WHERE s.relstorage NOT IN ('x', 'v', 'f');

GRANT SELECT ON __get_expect_files TO public;

--------------------------------------------------------------------------------
-- @view:
--        __get_expect_files_ext
--
-- @doc:
--        Retrieve a list of expected data files in the database,
--        using the knowledge from catalogs. This includes all
--        the extended data files for AO/CO tables, nor does it
--        include external, foreign or virtual tables.
--        Also ignore AO segments w/ eof=0. They might be created just for
--        modcount whereas no data has ever been inserted to the seg.
--        Or, they could be created when a seg has only aborted rows.
--        In both cases, we can ignore these segs, because no matter
--        whether the data files exist or not, the rest of the system
--        can handle them gracefully.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW __get_expect_files_ext AS
SELECT s.reltablespace AS tablespace, s.relname, s.relstorage,
       (CASE WHEN s.relfilenode != 0 THEN s.relfilenode ELSE pg_relation_filenode(s.oid) END)::text AS filename
FROM pg_class s 
WHERE s.relstorage NOT IN ('x', 'v', 'f')
UNION
-- AO extended files
SELECT c.reltablespace AS tablespace, c.relname, c.relstorage,
       format(c.relfilenode::text || '.' || s.segno::text) AS filename
FROM __get_ao_segno_list() s
JOIN pg_class c ON s.relid = c.oid
WHERE s.eof >0 AND c.relstorage NOT IN ('x', 'v', 'f')
UNION
-- CO extended files
SELECT c.reltablespace AS tablespace, c.relname, c.relstorage,
       format(c.relfilenode::text || '.' || s.segno::text) AS filename
FROM __get_aoco_segno_list() s
JOIN pg_class c ON s.relid = c.oid
WHERE s.eof > 0 AND c.relstorage NOT IN ('x', 'v', 'f');

GRANT SELECT ON __get_expect_files_ext TO public;

--------------------------------------------------------------------------------
-- @view:
--        __check_orphaned_files
--
-- @doc:
--        Check orphaned data files on default and user tablespaces.
--        A file is considered orphaned if its main relfilenode is not expected
--        to exist. For example, '12345.1' is an orphaned file if there is no
--        table has relfilenode=12345, but not otherwise.
--        Therefore, this view counts for file extension as well and we do not
--        need a "_ext" view like the missing file view.
--
--------------------------------------------------------------------------------
CREATE OR REPLACE VIEW __check_orphaned_files AS
SELECT f1.tablespace, f1.filename
from __get_exist_files f1
LEFT JOIN __get_expect_files f2
ON f1.tablespace = f2.tablespace AND substring(f1.filename from '[0-9]+') = f2.filename
WHERE f2.tablespace IS NULL
  AND f1.filename SIMILAR TO '[0-9]+(\.)?(\_)?%';

GRANT SELECT ON __check_orphaned_files TO public;

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
