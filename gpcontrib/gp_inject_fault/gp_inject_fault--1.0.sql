/* contrib/gp_fault_inject/gp_fault_inject--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_fault_inject" to load this file. \quit

CREATE FUNCTION gp_inject_fault(
  faultname text,
  type text,
  ddl text,
  database text,
  tablename text,
  start_occurrence int4,
  end_occurrence int4,
  extra_arg int4,
  db_id int4)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT NO SQL;

-- Simpler version, trigger only one time, occurrence start at 1 and
-- end at 1, no sleep and no ddl/database/tablename.
CREATE FUNCTION gp_inject_fault(
  faultname text,
  type text,
  db_id int4)
RETURNS text
AS $$ select gp_inject_fault($1, $2, '', '', '', 1, 1, 0, $3) $$
LANGUAGE SQL;

-- Simpler version, always trigger until fault is reset.
CREATE FUNCTION gp_inject_fault_infinite(
  faultname text,
  type text,
  db_id int4)
RETURNS text
AS $$ select gp_inject_fault($1, $2, '', '', '', 1, -1, 0, $3) $$
LANGUAGE SQL;

-- Simpler version to avoid confusion for wait_until_triggered fault.
-- occurrence in call below defines wait until number of times the
-- fault hits.
CREATE FUNCTION gp_wait_until_triggered_fault(
  faultname text,
  numtimestriggered int4,
  db_id int4)
RETURNS text
AS $$ select gp_inject_fault($1, 'wait_until_triggered', '', '', '', 1, 1, $2, $3) $$
LANGUAGE SQL;
