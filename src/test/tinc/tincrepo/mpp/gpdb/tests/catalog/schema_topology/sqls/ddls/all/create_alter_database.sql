-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology

\c db_test_bed

-- MPP-8466: set this GUC so that we can create database with latin8 encoding
-- 20100412: Ngoc
-- SET gp_check_locale_encoding_compatibility = off;
-- 20100414: Ngoc: GUC is not ported into 4.0 => remove ENCODING='latin8'

CREATE ROLE db_owner1;
CREATE ROLE db_owner2;

CREATE DATABASE db_name1 WITH OWNER = db_owner1 TEMPLATE =template1 CONNECTION LIMIT= 2;
--CREATE DATABASE db_name1 WITH OWNER = db_owner1 TEMPLATE =template1 ENCODING='latin8'  CONNECTION LIMIT= 2;
ALTER DATABASE db_name1 WITH  CONNECTION LIMIT 3;
ALTER DATABASE db_name1  RENAME TO new_db_name1;
ALTER DATABASE new_db_name1  OWNER TO db_owner2;
ALTER DATABASE new_db_name1 RENAME TO db_name1;

CREATE SCHEMA myschema;
ALTER DATABASE db_name1 SET search_path TO myschema, public, pg_catalog;
-- start_ignore
ALTER DATABASE db_name1 RESET search_path;
-- end_ignore
