-- Generate the file
\! python @script@ 10 0 > @data_dir@/exttab_union_1.tbl

-- Generate file with 2 errors
\! python @script@ 10 2 > @data_dir@/exttab_union_2.tbl

-- Error cases
-- Test: Should not allow table with incorrect columns / types for error tables

-- Incorrect column names
DROP TABLE IF EXISTS errtab1 cascade;

CREATE TABLE errtab1 (cmdtime timestamp with time zone, relname text, name text, 
linenm integer, bytenm integer, errmsg text, rawdata text, rawbytes bytea) distributed randomly; 

CREATE EXTERNAL TABLE exttab_error_basic( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS INTO errtab1 SEGMENT REJECT LIMIT 2;

-- Incorrect column types
DROP TABLE IF EXISTS errtab1;

CREATE TABLE errtab1(cmdtime timestamp with time zone, relname character varying, filename text, linenum numeric(5, 0), bytenum integer, errmsg text, rawdata text, rawbytes bytea) distributed randomly;

CREATE EXTERNAL TABLE exttab_error_basic( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS INTO errtab1 SEGMENT REJECT LIMIT 2;

-- Missing columns
DROP TABLE IF EXISTS errtab1;

CREATE TABLE errtab1(cmdtime timestamp with time zone, relname text, filename text, linenum integer, bytenum integer) distributed randomly;

CREATE EXTERNAL TABLE exttab_error_basic( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS INTO errtab1 SEGMENT REJECT LIMIT 2;

-- Incorrect distribution type
DROP TABLE IF EXISTS errtab1;

CREATE TABLE errtab1(cmdtime timestamp with time zone, relname text, filename text, linenum integer, bytenum integer, errmsg text, rawdata text, rawbytes bytea) distributed by (linenum);

CREATE EXTERNAL TABLE exttab_error_basic( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS INTO errtab1 SEGMENT REJECT LIMIT 2;

-- Partition table as error table
DROP TABLE IF EXISTS errtab1;

CREATE TABLE errtab1(cmdtime timestamp with time zone, relname text, filename text, linenum integer, bytenum integer, errmsg text, rawdata text, rawbytes bytea)
distributed randomly
partition by range(linenum)
(
start (0) inclusive end(300) every(100)
);

CREATE EXTERNAL TABLE exttab_error_basic( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS INTO errtab1 SEGMENT REJECT LIMIT 2;


-- Non-heap table as error table
DROP TABLE IF EXISTS errtab1;

CREATE TABLE errtab1(cmdtime timestamp with time zone, relname text, filename text, linenum integer, bytenum integer, errmsg text, rawdata text, rawbytes bytea)
WITH (appendonly=true, checksum=true) distributed randomly;

CREATE EXTERNAL TABLE exttab_error_basic( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS INTO errtab1 SEGMENT REJECT LIMIT 2;


-- ALTER table on error tables
DROP TABLE IF EXISTS errtab1_alter cascade;
DROP EXTERNAL TABLE IF EXISTS exttab_error_alter;

CREATE TABLE errtab1_alter(cmdtime timestamp with time zone, relname text, filename text, linenum integer, bytenum integer, errmsg text, rawdata text, rawbytes bytea) distributed randomly;

CREATE EXTERNAL TABLE exttab_error_alter( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_union_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS INTO errtab1_alter SEGMENT REJECT LIMIT 2;

-- ALTER TABLE should be disallowed
ALTER TABLE errtab1_alter DROP COLUMN relname;
ALTER TABLE errtab1_alter ADD COLUMN newcolumn integer;
ALTER TABLE errtab1_alter ALTER COLUMN errmsg TYPE varchar;
ALTER TABLE errtab1_alter ALTER COLUMN linenum SET DEFAULT 0;
ALTER TABLE errtab1_alter ALTER COLUMN cmdtime SET NOT NULL;
ALTER TABLE errtab1_alter ADD CONSTRAINT errtbl_chk_constraint CHECK (linenum < 1000);
ALTER TABLE errtab1_alter ADD PRIMARY KEY (cmdtime);
ALTER TABLE errtab1_alter OWNER TO @user@;

-- MPP-24478: ALTER distribution keys 
-- should be allowed to set distributed randomly as gpexpand uses this
ALTER TABLE errtab1_alter SET DISTRIBUTED RANDOMLY;
ALTER TABLE errtab1_alter SET WITH (reorganize=true) DISTRIBUTED RANDOMLY;
ALTER TABLE errtab1_alter SET WITH (reorganize=false) DISTRIBUTED RANDOMLY;
-- this should error out
ALTER TABLE errtab1_alter SET DISTRIBUTED BY (linenum);

-- ALTER TABLE SET schema should work
DROP SCHEMA IF EXISTS exttab_schema cascade;

CREATE SCHEMA exttab_schema;

ALTER TABLE errtab1_alter SET SCHEMA exttab_schema;

SET SEARCH_PATH=exttab_schema, public;

TRUNCATE TABLE errtab1_alter;

SELECT COUNT(*) FROM exttab_error_alter;

SELECT count(*) FROM errtab1_alter;

-- Rename of error table should work
DROP TABLE IF EXISTS new_errtab1_alter;
ALTER TABLE errtab1_alter RENAME TO new_errtab1_alter;

TRUNCATE TABLE new_errtab1_alter;

SELECT count(*) FROM exttab_error_alter;

SELECT count(*) FROM new_errtab1_alter;

