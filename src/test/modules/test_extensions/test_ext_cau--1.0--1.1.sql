/* src/test/modules/test_extensions/test_ext_cau--1.0--1.1.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "ALTER EXTENSION test_ext_cau" to load this file. \quit

ALTER EXTENSION test_ext_cau DROP function test_func1(int, int);
