/* src/test/modules/test_extensions/test_ext_cine--1.0--1.1.sql */
-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION test_ext_cine UPDATE TO '1.1'" to load this file. \quit

--
-- These are the same commands as in the 1.0 script; we expect them
-- to do nothing.
--

CREATE SCHEMA IF NOT EXISTS ext_cine_schema;

CREATE TABLE IF NOT EXISTS ext_cine_tab1 (x int);

-- just to verify the script ran
CREATE TABLE ext_cine_tab3 (z int);
