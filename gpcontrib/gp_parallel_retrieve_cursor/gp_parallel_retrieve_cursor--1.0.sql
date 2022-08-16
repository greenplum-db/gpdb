/*-------------------------------------------------------------------------
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates
 *
 * IDENTIFICATION
 *		gp_parallel_retrieve_cursor--1.0.sql
 *
 *-------------------------------------------------------------------------
 */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_parallel_retrieve_cursor" to load this file. \quit

-- to allow placement of objects inside pg_catalog
SET allow_system_table_mods TO on;

CREATE FUNCTION pg_catalog.gp_get_endpoints() RETURNS TABLE (gp_segment_id int4, auth_token text, cursorname text, sessionid int4, hostname varchar(64), port int4, username text, state text, endpointname text)
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT NO SQL;

CREATE FUNCTION pg_catalog.gp_get_segment_endpoints() RETURNS TABLE (auth_token text, databaseid oid, senderpid int4, receiverpid int4, state text, gp_segment_id oid, sessionid int4, username text, endpointname text, cursorname text)
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT NO SQL;

CREATE FUNCTION pg_catalog.gp_wait_parallel_retrieve_cursor(cursorname text, timeout_sec int4) RETURNS TABLE (finished bool)
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT NO SQL;

CREATE FUNCTION pg_catalog.gp_get_session_endpoints()
RETURNS TABLE (gp_segment_id int4, auth_token text, cursorname text, sessionid int4, hostname varchar(64), port int4, username text, state text, endpointname text) AS
$$
   SELECT * FROM pg_catalog.gp_get_endpoints()
        WHERE sessionid = (SELECT setting FROM pg_catalog.pg_settings WHERE name = 'gp_session_id')::int4
$$
LANGUAGE SQL EXECUTE ON MASTER;

COMMENT ON FUNCTION pg_catalog.gp_get_session_endpoints() IS 'All endpoints in this session that are visible to the current user.';

CREATE VIEW pg_catalog.gp_endpoints AS
    SELECT * FROM pg_catalog.gp_get_endpoints();

CREATE VIEW pg_catalog.gp_segment_endpoints AS
    SELECT * FROM pg_catalog.gp_get_segment_endpoints();

CREATE VIEW pg_catalog.gp_session_endpoints AS
    SELECT * FROM pg_catalog.gp_get_session_endpoints();

RESET allow_system_table_mods;
