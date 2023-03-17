/* src/test/modules/test_extensions/test_ext_cau--unpackaged--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION test_ext_cau FROM unpackaged" to load this file. \quit

ALTER EXTENSION test_ext_cau ADD function test_func1(int, int);
ALTER EXTENSION test_ext_cau ADD function test_func2(int, int);
