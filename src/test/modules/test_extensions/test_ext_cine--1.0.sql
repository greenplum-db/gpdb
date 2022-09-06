/* src/test/modules/test_extensions/test_ext_cine--1.0.sql */
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION test_ext_cine" to load this file. \quit

--
-- CREATE IF NOT EXISTS is an entirely unsound thing for an extension
-- to be doing, but let's at least plug the major security hole in it.
--

CREATE SCHEMA IF NOT EXISTS ext_cine_schema;

CREATE TABLE IF NOT EXISTS ext_cine_tab1 (x int);
