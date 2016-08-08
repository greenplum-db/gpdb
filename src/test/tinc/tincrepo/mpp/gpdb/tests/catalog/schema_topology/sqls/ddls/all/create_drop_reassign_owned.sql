-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Create roles, drop tables owned by different roles

\c db_test_bed
CREATE ROLE sally;
CREATE ROLE ron;
CREATE ROLE ken;
CREATE ROLE admin;

CREATE TABLE foo1 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo1 OWNER TO sally;
CREATE TABLE foo2 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo2 OWNER TO ron;
CREATE TABLE foo3 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo3 OWNER TO ken;

DROP OWNED by sally CASCADE;
DROP OWNED by ron RESTRICT;
DROP OWNED by ken;

CREATE TABLE foo1 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo1 OWNER TO sally;
CREATE TABLE foo2 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo2 OWNER TO ron;
CREATE TABLE foo3 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE foo3 OWNER TO ken;

REASSIGN OWNED BY sally,ron,ken to admin;
