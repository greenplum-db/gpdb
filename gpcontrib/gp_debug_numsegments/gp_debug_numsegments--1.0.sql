/* contrib/gp_debug_numsegments/gp_debug_numsegments--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION gp_debug_numsegments" to load this file. \quit

SET search_path = public;

-- This function set the default numsegments when creating tables.
-- This form accepts a text argument: 'full', 'minimal', 'random'.
CREATE OR REPLACE FUNCTION gp_debug_set_create_table_default_numsegments(text) RETURNS text
AS '$libdir/gp_debug_numsegments','gp_debug_set_create_table_default_numsegments'
LANGUAGE C STRICT;
-- This form accepts an integer argument: [1, gp_num_contents_in_cluster].
CREATE OR REPLACE FUNCTION gp_debug_set_create_table_default_numsegments(integer) RETURNS text
AS '$libdir/gp_debug_numsegments','gp_debug_set_create_table_default_numsegments'
LANGUAGE C STRICT;

-- This function reset the default numsegments.
-- This form accepts a text argument: 'full', 'minimal' or 'random',
-- numsegments will be reset to this value since this time.
CREATE OR REPLACE FUNCTION gp_debug_reset_create_table_default_numsegments(text) RETURNS void
AS '$libdir/gp_debug_numsegments','gp_debug_reset_create_table_default_numsegments'
LANGUAGE C STRICT;
-- This form accepts an integer argument: [1, gp_num_contents_in_cluster],
-- numsegments will be reset to this value since this time.
CREATE OR REPLACE FUNCTION gp_debug_reset_create_table_default_numsegments(integer) RETURNS void
AS '$libdir/gp_debug_numsegments','gp_debug_reset_create_table_default_numsegments'
LANGUAGE C STRICT;
-- This form resets numsegments directly, the value is decided by last reset,
-- or FULL if reset is never executed.
CREATE OR REPLACE FUNCTION gp_debug_reset_create_table_default_numsegments() RETURNS void
AS '$libdir/gp_debug_numsegments','gp_debug_reset_create_table_default_numsegments'
LANGUAGE C STRICT;

-- This function get the default numsegments when creating tables.
CREATE OR REPLACE FUNCTION gp_debug_get_create_table_default_numsegments() RETURNS text
AS '$libdir/gp_debug_numsegments','gp_debug_get_create_table_default_numsegments'
LANGUAGE C STRICT;
