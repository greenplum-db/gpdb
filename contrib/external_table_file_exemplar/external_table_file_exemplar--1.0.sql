/* contrib/external_table_file_exemplar/external_table_file_exemplar--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION external_table_file_exemplar" to load this file. \quit

CREATE FUNCTION external_table_file_handler()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION external_table_file_validator(text[], oid)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER external_table_file_exemplar
  HANDLER external_table_file_handler
  VALIDATOR external_table_file_validator;
