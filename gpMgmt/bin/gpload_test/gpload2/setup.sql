DROP DATABASE IF EXISTS reuse_gptest;

CREATE DATABASE reuse_gptest;

\c reuse_gptest

CREATE SCHEMA test;
CREATE SCHEMA "EXT_schema_test";

set client_min_messages='warning';
DROP EXTERNAL TABLE IF EXISTS temp_gpload_staging_table;
DROP TABLE IF EXISTS texttable;
DROP TABLE IF EXISTS csvtable;
DROP TABLE IF EXISTS test.csvtable;
DROP TABLE IF EXISTS texttable1;
DROP TABLE IF EXISTS testSpecialChar;
DROP TABLE IF EXISTS testheaderreuse;
DROP TABLE IF EXISTS chinese表;
DROP TABLE IF EXISTS testtruncate;
DROP TABLE IF EXISTS prices;
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
create table testSpecialChar("Field1" bigint, "Field#2" text) distributed by ("Field1");
CREATE TABLE testheaderreuse (
            field1            integer not null,
            field2            text,
            field3            text) DISTRIBUTED randomly;
CREATE TABLE chinese表 ( 列1 text, "列#2" int, lie3 timestamp, 列four decimal ) DISTRIBUTED BY ("列#2");
CREATE TABLE texttable2(s1 text, s2 text) DISTRIBUTED BY (s1);
CREATE TABLE testtruncate (
            s1 text, s2 text, s3 text, dt timestamp,
            n1 smallint, n2 integer, n3 bigint, n4 decimal,
            n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
INSERT INTO testtruncate VALUES('ttt','ttgt','shpits', '2011-06-01 12:30:30',16,732,834567,45.67,789.123,7.12345,156.456178);
CREATE TABLE prices (itemnumber integer, price decimal ) DISTRIBUTED BY (itemnumber);
CREATE TABLE testdk1 (c1 int, "C#2" text, "C#3" text) DISTRIBUTED BY ("C#2");
CREATE TABLE testdk2 (c1 int, "C#2" text, "C#3" text) DISTRIBUTED REPLICATED;
CREATE TABLE testdk3 (c1 int, "C#2" text, "C#3" text) DISTRIBUTED BY ("C#2", "c1");
