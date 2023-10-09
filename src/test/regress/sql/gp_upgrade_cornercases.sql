-- This file is run from not only pg_regress, but also during the standalone
-- pg_upgrade cluster test. It contains corner cases specific to pg_upgrade
-- where the state that needs to be tested would otherwise be destroyed during
-- a dump and restore (such as dropped columns).

DROP SCHEMA IF EXISTS upgrade_cornercases CASCADE;
CREATE SCHEMA upgrade_cornercases;
SET search_path TO upgrade_cornercases, public;

-- dropping columns on tables with inheritance
CREATE TABLE root1 (a int, b int, c int) DISTRIBUTED RANDOMLY;

CREATE TABLE child1 (d int) INHERITS (root1);

INSERT INTO root1 VALUES (1, 2, 3), (4, 5, 6), (7, 8, 9);
INSERT INTO child1
VALUES (10, 100, 1000, 10000),
       (11, 111, 1111, 11111),
       (12, 123, 1234, 12345);

ALTER TABLE root1 DROP COLUMN b;
ALTER TABLE ONLY root1 DROP COLUMN c;

-- adding child columns with name that clashes with dropped column
CREATE TABLE root2 (a int, b int, c int) DISTRIBUTED RANDOMLY;

INSERT INTO root2 VALUES (1, 2, 3), (4, 5, 6), (7, 8, 9);

ALTER TABLE root2 DROP COLUMN a;

CREATE TABLE child2 (a int, d int) INHERITS (root2);

INSERT INTO child2
VALUES (10, 100, 1000, 10000),
       (11, 111, 1111, 11111),
       (12, 123, 1234, 12345);

-- partitions with dropped columns
CREATE TABLE part (a int, b int, c int)
    DISTRIBUTED RANDOMLY
    PARTITION BY RANGE (a) (
        PARTITION part_1 START (0) INCLUSIVE END (3) EXCLUSIVE,
        DEFAULT PARTITION part_extra
);
INSERT INTO part VALUES (1, 2, 3), (4, 5, 6);

ALTER TABLE part DROP COLUMN c;
ALTER TABLE ONLY part DROP COLUMN b; -- note the ONLY here!

-- DROP all the columns
CREATE TABLE no_cols_left (a int, b int, c int) DISTRIBUTED RANDOMLY;

INSERT INTO no_cols_left VALUES (1, 2, 3), (4, 5, 6), (7, 8, 9);

ALTER TABLE no_cols_left DROP COLUMN a;
ALTER TABLE no_cols_left DROP COLUMN b;
ALTER TABLE no_cols_left DROP COLUMN c;

-- Test a database with a recreated "public" schema for upgrade test later.
-- Note that the public schema should be as vanilla as possible, so should expect no dependency on it for a new database.
-- If you are adding tests, you most likely want to add them BEFORE this, since
-- we connect to a different database.
DROP DATABASE IF EXISTS upgrade_recreated_public;
CREATE DATABASE upgrade_recreated_public;
\c upgrade_recreated_public

DROP SCHEMA public;
CREATE SCHEMA public;

CREATE TABLE public.t();
