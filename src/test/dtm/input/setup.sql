-- Install a helper function to inject faults, using the fault injection
-- mechanism built into the server.
CREATE FUNCTION gp_inject_fault(
  faultname text,
  type text,
  ddl text,
  database text,
  tableName text,
  numoccurrences int4,
  sleeptime int4)
RETURNS text
AS '@abs_builddir@/faultinject_extension@DLSUFFIX@'
LANGUAGE C VOLATILE STRICT NO SQL;
