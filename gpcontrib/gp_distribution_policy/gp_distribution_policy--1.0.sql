/* contrib/gp_distribution_policy/gp_distribution_policy--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_distribution_policy" to load this file. \quit

SET search_path = public;

-- This function validates the data distribution in a heap table in a segment.
CREATE OR REPLACE FUNCTION gp_distribution_policy_heap_table_check(oid, smallint[]) RETURNS boolean
AS '$libdir/gp_distribution_policy','gp_distribution_policy_heap_table_check'
LANGUAGE C IMMUTABLE STRICT;

-- This table function forces gp_distribution_policy_table_check to run in every segment through gp_id.
-- Every segment is checked for the correct data distribution.
CREATE OR REPLACE FUNCTION gp_heap_distribution_check(text) RETURNS TABLE (gp_segment_id int, is_distribution_correct boolean)
AS
$$
SELECT gp_execution_segment(),
        gp_distribution_policy_heap_table_check(
                (SELECT oid FROM pg_class WHERE relname =$1 AND relkind = 'r'),
                (SELECT attrnums FROM gp_distribution_policy WHERE localoid = (SELECT oid FROM pg_class WHERE relname =$1 AND relkind = 'r'))
        ) AS is_distribution_correct
$$
LANGUAGE SQL
EXECUTE ON ALL SEGMENTS;
