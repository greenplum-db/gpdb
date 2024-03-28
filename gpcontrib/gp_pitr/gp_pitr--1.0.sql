/* gpcontrib/gp_pitr/gp_pitr--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_pitr" to load this file. \quit

CREATE FUNCTION gp_set_recovery_pause_target(
    IN recovery_target_name text
)
    RETURNS void
AS '$libdir/gp_pitr', 'gp_set_recovery_pause_target'
LANGUAGE C VOLATILE;

COMMENT ON FUNCTION gp_set_recovery_pause_target(text) IS 'Set recovery target name to pause on';

REVOKE EXECUTE ON FUNCTION gp_set_recovery_pause_target(text) FROM public;
