/* contrib/pxf_fdw/pxf_fdw--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pxf_fdw" to load this file. \quit

CREATE FUNCTION pxf_fdw_handler()
    RETURNS fdw_handler
AS 'MODULE_PATHNAME'
    LANGUAGE C STRICT;

CREATE FUNCTION pxf_fdw_validator(text[], oid)
    RETURNS void
AS 'MODULE_PATHNAME'
    LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER pxf_fdw
  HANDLER pxf_fdw_handler
  VALIDATOR pxf_fdw_validator;

CREATE OR REPLACE FUNCTION pxf_fdw_version()
    RETURNS pg_catalog.int4 STRICT
AS 'MODULE_PATHNAME' LANGUAGE C;