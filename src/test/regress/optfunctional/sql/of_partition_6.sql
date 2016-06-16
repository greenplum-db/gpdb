
-- start_ignore
create schema sql_partition_251_300;
set search_path to sql_partition_251_300;
-- end_ignore
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_date;
CREATE TABLE mpp21090_drop_multicol_dml_date(
col1 date,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_date VALUES('2013-12-31',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_date DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_date DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_date SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_date SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_date WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_date;
CREATE TABLE mpp21090_drop_multicol_dml_date(
col1 date,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_date VALUES('2013-12-31',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_date DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_date DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_date SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_date SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_date WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_date;
CREATE TABLE mpp21090_drop_multicol_dml_date(
col1 date,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_date VALUES('2013-12-31',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_date DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_date DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_date SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_date SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_date WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_decimal;
CREATE TABLE mpp21090_drop_multicol_dml_decimal(
col1 decimal,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_decimal VALUES(2.00,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_decimal DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_decimal DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_decimal SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_decimal SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_decimal WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_decimal;
CREATE TABLE mpp21090_drop_multicol_dml_decimal(
col1 decimal,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_decimal VALUES(2.00,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_decimal DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_decimal DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_decimal SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_decimal SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_decimal WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_decimal;
CREATE TABLE mpp21090_drop_multicol_dml_decimal(
col1 decimal,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_decimal VALUES(2.00,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_decimal DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_decimal DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_decimal SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_decimal SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_decimal WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_float;
CREATE TABLE mpp21090_drop_multicol_dml_float(
col1 float,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_float VALUES(2.00,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_float DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_float DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_float SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_float SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_float WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_float;
CREATE TABLE mpp21090_drop_multicol_dml_float(
col1 float,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_float VALUES(2.00,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_float DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_float DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_float SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_float SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_float WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_float;
CREATE TABLE mpp21090_drop_multicol_dml_float(
col1 float,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_float VALUES(2.00,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_float DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_float DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_float SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_float SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_float WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_int4;
CREATE TABLE mpp21090_drop_multicol_dml_int4(
col1 int4,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_int4 VALUES(2000000000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_int4 DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_int4 DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_int4 SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_int4 SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_int4 WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_int4;
CREATE TABLE mpp21090_drop_multicol_dml_int4(
col1 int4,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_int4 VALUES(2000000000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_int4 DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_int4 DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_int4 SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_int4 SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_int4 WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_int4;
CREATE TABLE mpp21090_drop_multicol_dml_int4(
col1 int4,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_int4 VALUES(2000000000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_int4 DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_int4 DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_int4 SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_int4 SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_int4 WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_int8;
CREATE TABLE mpp21090_drop_multicol_dml_int8(
col1 int8,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_int8 VALUES(2000000000000000000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_int8 DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_int8 DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_int8 SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_int8 SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_int8 WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_int8;
CREATE TABLE mpp21090_drop_multicol_dml_int8(
col1 int8,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_int8 VALUES(2000000000000000000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_int8 DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_int8 DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_int8 SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_int8 SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_int8 WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_int8;
CREATE TABLE mpp21090_drop_multicol_dml_int8(
col1 int8,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_int8 VALUES(2000000000000000000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_int8 DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_int8 DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_int8 SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_int8 SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_int8 WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_int;
CREATE TABLE mpp21090_drop_multicol_dml_int(
col1 int,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_int VALUES(20000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_int DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_int DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_int SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_int SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_int WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_int;
CREATE TABLE mpp21090_drop_multicol_dml_int(
col1 int,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_int VALUES(20000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_int DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_int DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_int SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_int SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_int WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_int;
CREATE TABLE mpp21090_drop_multicol_dml_int(
col1 int,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_int VALUES(20000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_int DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_int DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_int SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_int SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_int WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_interval;
CREATE TABLE mpp21090_drop_multicol_dml_interval(
col1 interval,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_interval VALUES('1 day',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_interval DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_interval DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_interval SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_interval SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_interval WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_interval;
CREATE TABLE mpp21090_drop_multicol_dml_interval(
col1 interval,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_interval VALUES('1 day',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_interval DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_interval DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_interval SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_interval SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_interval WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_interval;
CREATE TABLE mpp21090_drop_multicol_dml_interval(
col1 interval,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_interval VALUES('1 day',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_interval DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_interval DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_interval SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_interval SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_interval WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_numeric;
CREATE TABLE mpp21090_drop_multicol_dml_numeric(
col1 numeric,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_numeric VALUES(2.000000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_numeric DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_numeric DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_numeric SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_numeric SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_numeric WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_numeric;
CREATE TABLE mpp21090_drop_multicol_dml_numeric(
col1 numeric,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_numeric VALUES(2.000000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_numeric DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_numeric DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_numeric SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_numeric SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_numeric WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_numeric;
CREATE TABLE mpp21090_drop_multicol_dml_numeric(
col1 numeric,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_numeric VALUES(2.000000,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_numeric DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_numeric DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_numeric SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_numeric SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_numeric WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_text;
CREATE TABLE mpp21090_drop_multicol_dml_text(
col1 text,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_text VALUES('abcdefghijklmnop',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_text DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_text DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_text SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_text SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_text WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_text;
CREATE TABLE mpp21090_drop_multicol_dml_text(
col1 text,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_text VALUES('abcdefghijklmnop',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_text DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_text DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_text SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_text SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_text WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_text;
CREATE TABLE mpp21090_drop_multicol_dml_text(
col1 text,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_text VALUES('abcdefghijklmnop',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_text DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_text DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_text SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_text SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_text WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_time;
CREATE TABLE mpp21090_drop_multicol_dml_time(
col1 time,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_time VALUES('12:00:00',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_time DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_time DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_time SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_time SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_time WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_time;
CREATE TABLE mpp21090_drop_multicol_dml_time(
col1 time,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_time VALUES('12:00:00',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_time DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_time DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_time SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_time SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_time WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_time;
CREATE TABLE mpp21090_drop_multicol_dml_time(
col1 time,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_time VALUES('12:00:00',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_time DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_time DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_time SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_time SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_time WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_timestamp;
CREATE TABLE mpp21090_drop_multicol_dml_timestamp(
col1 timestamp,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_timestamp VALUES('2013-12-31 12:00:00',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_timestamp DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_timestamp DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_timestamp SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_timestamp SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_timestamp WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_timestamp;
CREATE TABLE mpp21090_drop_multicol_dml_timestamp(
col1 timestamp,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_timestamp VALUES('2013-12-31 12:00:00',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_timestamp DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_timestamp DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_timestamp SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_timestamp SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_timestamp WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_timestamp;
CREATE TABLE mpp21090_drop_multicol_dml_timestamp(
col1 timestamp,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_timestamp VALUES('2013-12-31 12:00:00',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_timestamp DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_timestamp DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_timestamp SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_timestamp SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_timestamp WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_timestamptz;
CREATE TABLE mpp21090_drop_multicol_dml_timestamptz(
col1 timestamptz,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_timestamptz VALUES('2013-12-31 12:00:00 PST',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_timestamptz DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_timestamptz DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_timestamptz SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_timestamptz SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_timestamptz WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_timestamptz;
CREATE TABLE mpp21090_drop_multicol_dml_timestamptz(
col1 timestamptz,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_timestamptz VALUES('2013-12-31 12:00:00 PST',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_timestamptz DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_timestamptz DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_timestamptz SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_timestamptz SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_timestamptz WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_timestamptz;
CREATE TABLE mpp21090_drop_multicol_dml_timestamptz(
col1 timestamptz,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_timestamptz VALUES('2013-12-31 12:00:00 PST',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_timestamptz DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_timestamptz DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_timestamptz SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_timestamptz SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_timestamptz WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_char;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_char VALUES('x','x','a','x');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_char DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_char VALUES('x','x','b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_char ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_char ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_char VALUES('x','x','c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_char SPLIT DEFAULT PARTITION at ('d') into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_char SELECT 'a', 'a','e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_char ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_char SET col1 = 'z' WHERE col2 = 'a' AND col1 = 'a';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_char SET col2 = 'z' WHERE col2 = 'a' AND col1 = 'z';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_char WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_char ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_date;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 date
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_date VALUES('2013-12-31','2013-12-31','a','2013-12-31');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_date DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_date VALUES('2013-12-31','2013-12-31','b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_date ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_date ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_date VALUES('2013-12-31','2013-12-31','c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_date SPLIT DEFAULT PARTITION at ('2013-12-15') into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_date SELECT '2013-12-01', '2013-12-01','e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_date ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_date SET col1 = '2014-02-10' WHERE col2 = '2013-12-01' AND col1 = '2013-12-01';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_date SET col2 = '2014-02-10' WHERE col2 = '2013-12-01' AND col1 = '2014-02-10';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_date WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_date ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_decimal;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 decimal
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_decimal VALUES(2.00,2.00,'a',2.00);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_decimal DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_decimal VALUES(2.00,2.00,'b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_decimal ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_decimal ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_decimal VALUES(2.00,2.00,'c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_decimal SPLIT DEFAULT PARTITION at (5.00) into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_decimal SELECT 1.00, 1.00,'e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_decimal ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_decimal SET col1 = 35.00 WHERE col2 = 1.00 AND col1 = 1.00;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_decimal SET col2 = 35.00 WHERE col2 = 1.00 AND col1 = 35.00;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_decimal WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_decimal ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_float;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 float
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_float VALUES(2.00,2.00,'a',2.00);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_float ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_float DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_float VALUES(2.00,2.00,'b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_float ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_float ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_float VALUES(2.00,2.00,'c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_float ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_float SPLIT DEFAULT PARTITION at (5.00) into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_float SELECT 1.00, 1.00,'e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_float ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_float SET col1 = 35.00 WHERE col2 = 1.00 AND col1 = 1.00;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_float SET col2 = 35.00 WHERE col2 = 1.00 AND col1 = 35.00;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_float ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_float WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_float ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_int;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int VALUES(20000,20000,'a',20000);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int VALUES(20000,20000,'b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int VALUES(20000,20000,'c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int SPLIT DEFAULT PARTITION at (5000) into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int SELECT 1000, 1000,'e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_int SET col1 = 35000 WHERE col2 = 1000 AND col1 = 1000;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_int SET col2 = 35000 WHERE col2 = 1000 AND col1 = 35000;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_int WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_int4;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int4
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int4 VALUES(20000000,20000000,'a',20000000);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int4 DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int4 VALUES(20000000,20000000,'b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int4 ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int4 VALUES(20000000,20000000,'c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int4 SPLIT DEFAULT PARTITION at (50000000) into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int4 SELECT 10000000, 10000000,'e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_int4 SET col1 = 35000000 WHERE col2 = 10000000 AND col1 = 10000000;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_int4 SET col2 = 35000000 WHERE col2 = 10000000 AND col1 = 35000000;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int4 ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_int8;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int8
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int8 VALUES(2000000000000000000,2000000000000000000,'a',2000000000000000000);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int8 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int8 DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int8 VALUES(2000000000000000000,2000000000000000000,'b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int8 ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int8 ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int8 VALUES(2000000000000000000,2000000000000000000,'c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int8 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_int8 SPLIT DEFAULT PARTITION at (500000000000000000) into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_int8 SELECT 100000000000000000, 100000000000000000,'e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int8 ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_int8 SET col1 = 3500000000000000000 WHERE col2 = 100000000000000000 AND col1 = 100000000000000000;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_int8 SET col2 = 3500000000000000000 WHERE col2 = 100000000000000000 AND col1 = 3500000000000000000;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_int8 WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_int8 ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_interval;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 interval
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_interval VALUES('1 hour','1 hour','a','1 hour');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_interval DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_interval VALUES('1 hour','1 hour','b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_interval ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_interval ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_interval VALUES('1 hour','1 hour','c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_interval SPLIT DEFAULT PARTITION at ('30 secs') into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_interval SELECT '1 sec', '1 sec','e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_interval ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_interval SET col1 = '14 hours' WHERE col2 = '1 sec' AND col1 = '1 sec';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_interval SET col2 = '14 hours' WHERE col2 = '1 sec' AND col1 = '14 hours';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_interval WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_interval ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_numeric;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_numeric VALUES(2.000000,2.000000,'a',2.000000);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_numeric ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_numeric DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_numeric VALUES(2.000000,2.000000,'b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_numeric ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_numeric ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_numeric VALUES(2.000000,2.000000,'c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_numeric ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_numeric SPLIT DEFAULT PARTITION at (5.000000) into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_numeric SELECT 1.000000, 1.000000,'e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_numeric ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_numeric SET col1 = 35.000000 WHERE col2 = 1.000000 AND col1 = 1.000000;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_numeric SET col2 = 35.000000 WHERE col2 = 1.000000 AND col1 = 35.000000;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_numeric WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_numeric ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_time;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_time VALUES('12:00:00','12:00:00','a','12:00:00');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_time ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_time DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_time VALUES('12:00:00','12:00:00','b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_time ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_time ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_time VALUES('12:00:00','12:00:00','c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_time ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_time SPLIT DEFAULT PARTITION at ('15:00:00') into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_time SELECT '12:00:00', '12:00:00','e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_time ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_time SET col1 = '11:30:00' WHERE col2 = '12:00:00' AND col1 = '12:00:00';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_time SET col2 = '11:30:00' WHERE col2 = '12:00:00' AND col1 = '11:30:00';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_time WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_time ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_timestamp;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 timestamp
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a','2013-12-31 12:00:00');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp SPLIT DEFAULT PARTITION at ('2013-12-15 12:00:00') into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamp SELECT '2013-12-01 12:00:00', '2013-12-01 12:00:00','e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp SET col1 = '2014-02-10 12:00:00' WHERE col2 = '2013-12-01 12:00:00' AND col1 = '2013-12-01 12:00:00';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_timestamp SET col2 = '2014-02-10 12:00:00' WHERE col2 = '2013-12-01 12:00:00' AND col1 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamp ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz;
CREATE TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 timestamptz
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a','2013-12-31 12:00:00 PST');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','b');
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz ORDER BY 1,2,3;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz SPLIT DEFAULT PARTITION at ('2013-12-15 12:00:00 PST') into (partition partsplitone,partition def);

INSERT INTO mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz SELECT '2013-12-01 12:00:00 PST', '2013-12-01 12:00:00 PST','e', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz SET col1 = '2014-02-10 12:00:00 PST' WHERE col2 = '2013-12-01 12:00:00 PST' AND col1 = '2013-12-01 12:00:00 PST';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz SET col2 = '2014-02-10 12:00:00 PST' WHERE col2 = '2013-12-01 12:00:00 PST' AND col1 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitdefpt_dml_timestamptz ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitpt_dml_char;
CREATE TABLE mpp21090_dropcol_addcol_splitpt_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_char VALUES('x','x','a','x');
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitpt_dml_char ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_char VALUES('x','x','b','x',0);
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitpt_dml_char DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_char VALUES('x','x','c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitpt_dml_char SPLIT PARTITION partone at ('d') into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_char SELECT 'a', 'a','d', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_char ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitpt_dml_char SET col1 = 'z' WHERE col2 = 'a' AND col1 = 'a';
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitpt_dml_char SET col2 ='x'  WHERE col2 = 'a' AND col1 = 'z';
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitpt_dml_char WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_char ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_addcol_splitpt_dml_date;
CREATE TABLE mpp21090_dropcol_addcol_splitpt_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 date
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_date VALUES('2013-12-31','2013-12-31','a','2013-12-31');
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitpt_dml_date ADD COLUMN col5 int DEFAULT 10;

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_date VALUES('2013-12-31','2013-12-31','b','2013-12-31',0);
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitpt_dml_date DROP COLUMN col4;

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_date VALUES('2013-12-31','2013-12-31','c',1);
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_addcol_splitpt_dml_date SPLIT PARTITION partone at ('2013-12-15') into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_addcol_splitpt_dml_date SELECT '2013-12-01', '2013-12-01','d', 1;
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_date ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_addcol_splitpt_dml_date SET col1 = '2014-02-10' WHERE col2 = '2013-12-01' AND col1 = '2013-12-01';
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_addcol_splitpt_dml_date SET col2 ='2013-12-31'  WHERE col2 = '2013-12-01' AND col1 = '2014-02-10';
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_addcol_splitpt_dml_date WHERE col3='b';
SELECT * FROM mpp21090_dropcol_addcol_splitpt_dml_date ORDER BY 1,2,3;

-- start_ignore
drop schema sql_partition_251_300 cascade;
-- end_ignore
