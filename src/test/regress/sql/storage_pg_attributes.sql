drop table if exists abuela;
CREATE TABLE abuela (a int) USING ao_row;
-- verify that cmax, xmax, cmin, and xmin do not exist
SELECT attrelid::regclass, attname, attnum FROM pg_attribute WHERE attrelid = 'abuela'::regclass;
INSERT INTO abuela VALUES (0);
-- show parser fails
SELECT count(xmin) FROM abuela;

drop table if exists abuela;
CREATE TABLE abuela (a int) USING ao_column;
-- verify that cmax, xmax, cmin, and xmin do not exist
SELECT attrelid::regclass, attname, attnum FROM pg_attribute WHERE attrelid = 'abuela'::regclass;
INSERT INTO abuela VALUES (0);
-- show parser fails
SELECT count(xmin) FROM abuela;

drop table if exists abuela;
CREATE TABLE abuela (a int);
-- verify that cmax, xmax, cmin, xmin exists
SELECT attrelid::regclass, attname, attnum FROM pg_attribute WHERE attrelid = 'abuela'::regclass;
INSERT INTO abuela VALUES (0);
-- show parser pass
SELECT count(xmin) FROM abuela;
