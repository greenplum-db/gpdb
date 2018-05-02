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

-- Simpler version to avoid confusion for wait_until_triggered fault.
-- occurrence in call below defines wait until number of times the
-- fault hits.
CREATE FUNCTION gp_wait_until_triggered_fault(
  faultname text,
  numtimestriggered int4,
  db_id int4)
RETURNS boolean
AS $$ select gp_inject_fault($1, 'wait_until_triggered', '', '', '', $2, 0, $3) $$
LANGUAGE SQL;
