\echo Use "CREATE EXTENSION gp_replica_check" to load this file. \quit
CREATE FUNCTION gp_replica_check(primarydirpath text, mirrordirpath text, include_types text) RETURNS boolean
AS '$libdir/gp_replica_check', 'gp_replica_check'
LANGUAGE C STRICT;
