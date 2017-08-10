/* contrib/gp_fault_inject/gp_fault_inject--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_fault_inject" to load this file. \quit

CREATE FUNCTION gp_inject_fault(
  faultname text,
  type text,
  ddl text,
  database text,
  tablename text,
  numoccurrences int4,
  sleeptime int4,
  db_id int4)
RETURNS boolean
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT NO SQL;

-- Simpler version, for convenience in the common case of 1 occurrence,
-- no sleep, and no ddl/database/tablename.
CREATE FUNCTION gp_inject_fault(
  faultname text,
  type text,
  db_id int4)
RETURNS boolean
AS $$ select gp_inject_fault($1, $2, '', '', '', 1, 0, $3) $$
LANGUAGE SQL;
