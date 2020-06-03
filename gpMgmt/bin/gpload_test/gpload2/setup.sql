DROP DATABASE IF EXISTS reuse_gptest;

CREATE DATABASE reuse_gptest;

\c reuse_gptest

CREATE SCHEMA test;

set client_min_messages='warning';
DROP EXTERNAL TABLE IF EXISTS temp_gpload_staging_table;
DROP TABLE IF EXISTS texttable;
DROP TABLE IF EXISTS csvtable;
DROP TABLE IF EXISTS texttable1;
reset client_min_messages;
CREATE TABLE texttable (
            s1 text, s2 text, s3 text, dt timestamp,
            n1 smallint, n2 integer, n3 bigint, n4 decimal,
            n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
CREATE TABLE texttable1 (
            s1 text, s2 text, s3 text, dt timestamp,
            n1 smallint, n2 integer, n3 bigint, n4 decimal,
            n5 numeric, n6 real, n7 double precision, n8 int) DISTRIBUTED BY (n1);
CREATE TABLE csvtable (
	    year int, make text, model text, decription text, price decimal)
            DISTRIBUTED BY (year);
CREATE TABLE test.csvtable (
	    year int, make text, model text, decription text, price decimal)
            DISTRIBUTED BY (year);
