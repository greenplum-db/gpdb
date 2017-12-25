DROP DATABASE IF EXISTS reuse_gptest;

CREATE DATABASE reuse_gptest;

\c reuse_gptest

CREATE SCHEMA test;

DROP TABLE IF EXISTS texttable;
DROP TABLE IF EXISTS csvtable;
CREATE TABLE texttable (
            s1 text, s2 text, s3 text, dt timestamp,
            n1 smallint, n2 integer, n3 bigint, n4 decimal,
            n5 numeric, n6 real, n7 double precision) DISTRIBUTED BY (n1);
CREATE TABLE csvtable (
	    year int, make text, model text, decription text, price decimal)
            DISTRIBUTED BY (year);
