
-- start_ignore
create schema triggers;
set search_path to triggers;
-- end_ignore
-- start_ignore
DROP TABLE IF EXISTS dml_trigger_table;

CREATE TABLE dml_trigger_table
(
 name varchar(10),
 age  numeric(10),
 updated_by varchar
)
distributed by (age)
partition by range (age) (
	partition p1 start (1) end (25),
	partition p2 start (26) end (50),
	partition p3 start (51) end (75)
	);

CREATE OR REPLACE FUNCTION dml_function_1() RETURNS trigger AS
$$
BEGIN
   NEW.updated_by = 'a';
   RETURN NEW;
END
$$ LANGUAGE 'plpgsql';

CREATE TRIGGER dml_trigger_1
BEFORE INSERT or UPDATE
ON dml_trigger_table_1_prt_p1
FOR EACH ROW
EXECUTE PROCEDURE dml_function_1();-- end_ignore
--start_ignore
SET client_min_messages='log';
INSERT INTO dml_trigger_table VALUES('TEST',10);
SET client_min_messages='notice';
--end_ignore

SELECT * FROM dml_trigger_table order by 2;

-- start_ignore
DROP TABLE IF EXISTS dml_trigger_table_1;
CREATE TABLE dml_trigger_table_1
(
 name varchar(10),
 age  numeric(10)
)
distributed by (age);

INSERT INTO dml_trigger_table_1 VALUES('TEST',10);

CREATE OR REPLACE FUNCTION dml_function_1() RETURNS trigger AS
$$
BEGIN
   RETURN OLD;
END
$$ LANGUAGE 'plpgsql';

CREATE TRIGGER dml_trigger_1
BEFORE UPDATE or DELETE
ON dml_trigger_table_1
FOR EACH ROW
EXECUTE PROCEDURE dml_function_1();-- end_ignore
SELECT * FROM dml_trigger_table_1 order by 2;

--start_ignore
SET client_min_messages='log';
DELETE FROM dml_trigger_table_1 where age=10;
SET client_min_messages='notice';
--end_ignore

SELECT * FROM dml_trigger_table_1 order by 2;

-- start_ignore
DROP TABLE IF EXISTS dml_trigger_table_1;
CREATE TABLE dml_trigger_table_1
(
 name varchar(10),
 age  numeric(10)
)
distributed by (age);

INSERT INTO dml_trigger_table_1 VALUES('TEST',10);

CREATE OR REPLACE FUNCTION dml_function_1() RETURNS trigger AS
$$
BEGIN
   RETURN OLD;
END
$$ LANGUAGE 'plpgsql';

CREATE TRIGGER dml_trigger_1
BEFORE UPDATE or INSERT
ON dml_trigger_table_1
FOR EACH ROW
EXECUTE PROCEDURE dml_function_1();-- end_ignore
SELECT * FROM dml_trigger_table_1 order by 2;

--start_ignore
SET client_min_messages='log';
DELETE FROM dml_trigger_table_1 where age=10;
SET client_min_messages='notice';
--end_ignore

SELECT * FROM dml_trigger_table_1 order by 2;

-- start_ignore
DROP TABLE IF EXISTS dml_trigger_table_1;
CREATE TABLE dml_trigger_table_1
(
 name varchar(10),
 age  numeric(10),
 updated_by varchar
)
distributed by (age);

CREATE OR REPLACE FUNCTION dml_function_1() RETURNS trigger AS
$$
BEGIN
   NEW.updated_by = 'a';
   RETURN NEW;
END
$$ LANGUAGE 'plpgsql';

CREATE TRIGGER dml_trigger_1
BEFORE INSERT or UPDATE
ON dml_trigger_table_1
FOR EACH ROW
EXECUTE PROCEDURE dml_function_1();-- end_ignore
--start_ignore
SET client_min_messages='log';
INSERT INTO dml_trigger_table_1 VALUES('TEST',10);
SET client_min_messages='notice';
--end_ignore

SELECT * FROM dml_trigger_table_1 order by 2;

-- start_ignore
DROP TABLE IF EXISTS dml_trigger_table_1;
CREATE TABLE dml_trigger_table_1
(
 name varchar(10),
 age  numeric(10),
 updated_by varchar
)
distributed by (age);

CREATE OR REPLACE FUNCTION dml_function_1() RETURNS trigger AS
$$
BEGIN
   NEW.updated_by = 'a';
   RETURN NEW;
END
$$ LANGUAGE 'plpgsql';

CREATE TRIGGER dml_trigger_1
AFTER DELETE or UPDATE
ON dml_trigger_table_1
FOR EACH ROW
EXECUTE PROCEDURE dml_function_1();-- end_ignore
--start_ignore
SET client_min_messages='log';
INSERT INTO dml_trigger_table_1 VALUES('TEST',10);
SET client_min_messages='notice';
--end_ignore

SELECT * FROM dml_trigger_table_1 order by 2;

-- start_ignore
DROP TABLE IF EXISTS dml_trigger_table_1;
CREATE TABLE dml_trigger_table_1
(
 name varchar(10),
 age  numeric(10),
 updated_by varchar
)
distributed by (age);

INSERT INTO dml_trigger_table_1 VALUES('TEST',10);

CREATE OR REPLACE FUNCTION dml_function_1() RETURNS trigger AS
$$
BEGIN
   NEW.updated_by = 'a';
   RETURN NEW;
END
$$ LANGUAGE 'plpgsql';

CREATE TRIGGER dml_trigger_1
BEFORE INSERT or UPDATE
ON dml_trigger_table_1
FOR EACH ROW
EXECUTE PROCEDURE dml_function_1();-- end_ignore
SELECT * FROM dml_trigger_table_1 order by 2;

--start_ignore
SET client_min_messages='log';
UPDATE dml_trigger_table_1 set name='NEW TEST' where name='TEST';
SET client_min_messages='notice';
--end_ignore

SELECT * FROM dml_trigger_table_1 order by 2;

-- start_ignore
DROP TABLE IF EXISTS dml_trigger_table_1;
CREATE TABLE dml_trigger_table_1
(
 name varchar(10),
 age  numeric(10),
 updated_by varchar
)
distributed by (age);

INSERT INTO dml_trigger_table_1 VALUES('TEST',10);

CREATE OR REPLACE FUNCTION dml_function_1() RETURNS trigger AS
$$
BEGIN
   NEW.updated_by = 'a';
   RETURN NEW;
END
$$ LANGUAGE 'plpgsql';

CREATE TRIGGER dml_trigger_1
BEFORE INSERT or DELETE
ON dml_trigger_table_1
FOR EACH ROW
EXECUTE PROCEDURE dml_function_1();-- end_ignore
SELECT * FROM dml_trigger_table_1 order by 2;

--start_ignore
SET client_min_messages='log';
UPDATE dml_trigger_table_1 set name='NEW TEST' where name='TEST';
SET client_min_messages='notice';
--end_ignore

SELECT * FROM dml_trigger_table_1 order by 2;


-- start_ignore
drop schema triggers cascade;
-- end_ignore
