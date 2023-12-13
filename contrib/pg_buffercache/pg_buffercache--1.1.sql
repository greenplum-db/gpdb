/* contrib/pg_buffercache/pg_buffercache--1.1.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_buffercache" to load this file. \quit

-- Register the function.
CREATE FUNCTION pg_buffercache_pages()
RETURNS SETOF RECORD
AS 'MODULE_PATHNAME', 'pg_buffercache_pages'
LANGUAGE C;

-- Create a view for convenient access.
CREATE VIEW pg_buffercache AS
	SELECT P.* FROM pg_buffercache_pages() AS P
	(bufferid integer, relfilenode oid, reltablespace oid, reldatabase oid,
	 relforknumber int2, relblocknumber int8, isdirty bool, usagecount int2,
	 pinning_backends int4);

CREATE VIEW gp_buffercache AS
	SELECT gp_execution_segment() AS gp_segment_id, *
	FROM gp_dist_random('pg_buffercache')
	UNION ALL
	SELECT -1 AS gp_segment_id, *
  FROM pg_buffercache
  ORDER BY 1,2;

-- Don't want these to be available to public.
REVOKE ALL ON FUNCTION pg_buffercache_pages() FROM PUBLIC;
REVOKE ALL ON pg_buffercache FROM PUBLIC;
REVOKE ALL ON gp_buffercache FROM PUBLIC;
