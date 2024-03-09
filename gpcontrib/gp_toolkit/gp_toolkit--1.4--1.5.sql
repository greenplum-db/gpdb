/* gpcontrib/gp_toolkit/gp_toolkit--1.4--1.5.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION gp_toolkit UPDATE TO '1.5" to load this file. \quit

-- Check orphaned data files on default and user tablespaces.
-- Compared to the previous version, add gp_segment_id to show which segment it is being executed.
CREATE OR REPLACE VIEW gp_toolkit.__check_orphaned_files AS
SELECT f1.tablespace, f1.filename, f1.filepath, pg_catalog.gp_execution_segment() AS gp_segment_id
from gp_toolkit.__get_exist_files f1
LEFT JOIN gp_toolkit.__get_expect_files f2
ON f1.tablespace = f2.tablespace AND substring(f1.filename from '[0-9]+') = f2.filename
WHERE f2.tablespace IS NULL
  AND f1.filename SIMILAR TO '(t_)*[0-9]+(\.)?(\_)?%';
