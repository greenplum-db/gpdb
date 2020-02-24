/* contrib/pxf_fdw/pxf_fdw--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pxf_fdw" to load this file. \quit

CREATE FUNCTION pxf_fdw_handler()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION pxf_fdw_validator(text[], oid)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER jdbc_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'jdbc', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER hdfs_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'hdfs', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER hive_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'hive', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER hbase_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'hbase', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER s3_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 's3', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER gs_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'gs', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER adl_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'adl', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER wasbs_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'wasbs', mpp_execute 'all segments' );

CREATE FOREIGN DATA WRAPPER file_pxf_fdw
    HANDLER pxf_fdw_handler
    VALIDATOR pxf_fdw_validator
    OPTIONS ( protocol 'localfile', mpp_execute 'all segments' );