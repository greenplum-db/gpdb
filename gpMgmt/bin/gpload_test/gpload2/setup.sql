DROP DATABASE IF EXISTS reuse_gptest;

CREATE DATABASE reuse_gptest;

\c reuse_gptest

CREATE SCHEMA "Test";
CREATE SCHEMA test;

DROP EXTERNAL TABLE IF EXISTS temp_gpload_staging_table;
DROP TABLE IF EXISTS texttable;
DROP TABLE IF EXISTS csvtable;
DROP TABLE IF EXISTS testSpecialChar;
DROP TABLE IF EXISTS testheaderreuse;

CREATE TABLE texttable (
            s1 text, s2 text, s3 text, dt timestamp,
            n1 smallint, n2 integer, n3 bigint, n4 decimal,
            n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
CREATE TABLE csvtable (
	    year int, make text, model text, decription text, price decimal)
            DISTRIBUTED BY (year);
CREATE TABLE test.csvtable (
	    year int, make text, model text, decription text, price decimal)
            DISTRIBUTED BY (year);
CREATE TABLE testSpecialChar("Field1" bigint, "Field#2" text) DISTRIBUTED BY ("Field1");
CREATE TABLE testheaderreuse (
            field1            integer not null,
            field2            text,
            field3            text) DISTRIBUTED randomly;
