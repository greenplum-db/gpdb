/* contrib/pg_buffercache/pg_buffercache--1.0--1.1.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION pg_buffercache UPDATE TO '1.1'" to load this file. \quit

-- Upgrade view to 1.1. format
CREATE OR REPLACE VIEW pg_buffercache AS
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

REVOKE ALL ON gp_buffercache FROM PUBLIC;
