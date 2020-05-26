/* gpcontrib/gp_exttable_fdw/gp_exttable_fdw--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_exttable_fdw" to load this file. \quit

CREATE FUNCTION gp_exttable_fdw_handler()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION gp_exttable_validator(text[], oid)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER gp_exttable_fdw
    HANDLER gp_exttable_fdw_handler
    VALIDATOR gp_exttable_validator;

CREATE SERVER gp_exttable_server FOREIGN DATA WRAPPER gp_exttable_fdw;

-- Functions to extract external table info
CREATE FUNCTION pg_exttable(OUT reloid oid,
                            OUT urilocation text[],
                            OUT execlocation text[],
                            OUT fmttype "char",
                            OUT fmtopts text,
                            OUT options text[],
                            OUT command text,
                            OUT rejectlimit int4,
                            OUT rejectlimittype "char",
                            OUT logerrors "char",
                            OUT encoding int4,
                            OUT writable bool)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE EXECUTE ON MASTER;

CREATE VIEW pg_exttable AS SELECT * FROM pg_exttable();
