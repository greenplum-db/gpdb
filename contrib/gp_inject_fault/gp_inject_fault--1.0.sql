/* contrib/gp_fault_inject/gp_fault_inject--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_fault_inject" to load this file. \quit

CREATE FUNCTION gp_inject_fault_new(
  faultname text,
  type text,
  ddl text,
  database text,
  tablename text,
  start_occurrence int4,
  end_occurrence int4,
  extra_arg int4,
  db_id int4)
RETURNS boolean
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT NO SQL;

-- Simpler version, trigger only one time, occurrence start at 1 and
-- end at 1, no sleep and no ddl/database/tablename.  If you are
-- porting a test from master, replace gp_inject_fault() from master
-- with gp_inject_fault_new().
CREATE FUNCTION gp_inject_fault_new(
  faultname text,
  type text,
  db_id int4)
RETURNS boolean
AS $$ select gp_inject_fault_new($1, $2, '', '', '', 1, 1, 0, $3) $$
LANGUAGE SQL;

-- Old version, always trigger until fault is reset.  This is kept to
-- avoid modifications to existing tests.  This is identical to the
-- newly introduced *_infinite() version in master.
CREATE FUNCTION gp_inject_fault(
  faultname text,
  type text,
  db_id int4)
RETURNS boolean
AS $$ select gp_inject_fault_new($1, $2, '', '', '', 1, -1, 0, $3) $$
LANGUAGE SQL;

-- Old version, always trigger until fault is reset.  The old
-- interface accepted (1) number of occurrences after which the fault
-- will take affect and (2) sleeptime which maps to extraArg.
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
AS $$ select gp_inject_fault_new($1, $2, $3, $4, $5, $6, -1, $7, $8) $$
LANGUAGE SQL;

-- Simpler version, always trigger until fault is reset.
CREATE FUNCTION gp_inject_fault_infinite(
  faultname text,
  type text,
  db_id int4)
RETURNS boolean
AS $$ select gp_inject_fault_new($1, $2, '', '', '', 1, -1, 0, $3) $$
LANGUAGE SQL;

-- Simpler version to avoid confusion for wait_until_triggered fault.
-- occurrence in call below defines wait until number of times the
-- fault hits.
CREATE FUNCTION gp_wait_until_triggered_fault(
  faultname text,
  numtimestriggered int4,
  db_id int4)
RETURNS boolean
AS $$ select gp_inject_fault_new($1, 'wait_until_triggered', '', '', '', 1, 1, $2, $3) $$
LANGUAGE SQL;

