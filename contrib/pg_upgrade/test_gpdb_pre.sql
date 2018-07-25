-- WARNING
-- This file is executed against the postgres database, as that is known to
-- exist at the time of running upgrades. If objects are to be manipulated
-- in other databases, make sure to change to the correct database first.

DROP DATABASE regression;
DROP DATABASE dsp3;
DROP DATABASE isolation2test;

CREATE DATABASE pgutest;
\c pgutest;

CREATE TABLE t (a integer, b numeric);
CREATE TALBE tt (a integer, b numeric) WITH (appendonly = true);
