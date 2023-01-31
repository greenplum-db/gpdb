/* src/test/modules/test_extensions/test_ext_cau--1.1.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION test_ext_cau" to load this file. \quit

create function test_func2(a int, b int) returns int
as $$
begin
	return a - b;
end;
$$
LANGUAGE plpgsql;
