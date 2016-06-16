
-- start_ignore
create schema sql_partition_201_250;
set search_path to sql_partition_201_250;
-- end_ignore
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_lastcol_index_dml_timestamptz;
CREATE TABLE mpp21090_drop_lastcol_index_dml_timestamptz(
col1 int,
col2 decimal,
col3 char,
col4 date,
col5 timestamptz
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col3);

DROP INDEX IF EXISTS mpp21090_drop_lastcol_index_dml_idx_timestamptz;
CREATE INDEX mpp21090_drop_lastcol_index_dml_idx_timestamptz on mpp21090_drop_lastcol_index_dml_timestamptz(col3);

INSERT INTO mpp21090_drop_lastcol_index_dml_timestamptz VALUES(0,0.00,'a','2014-01-01','2013-12-30 12:00:00 PST');
SELECT * FROM mpp21090_drop_lastcol_index_dml_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_drop_lastcol_index_dml_timestamptz DROP COLUMN col5;

INSERT INTO mpp21090_drop_lastcol_index_dml_timestamptz SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_lastcol_index_dml_timestamptz ORDER BY 1,2,3,4;

UPDATE mpp21090_drop_lastcol_index_dml_timestamptz SET col3='c' WHERE col3 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_lastcol_index_dml_timestamptz ORDER BY 1,2,3,4;

DELETE FROM mpp21090_drop_lastcol_index_dml_timestamptz WHERE col3='c';
SELECT * FROM mpp21090_drop_lastcol_index_dml_timestamptz ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_lastcol_index_dml_timestamptz;
CREATE TABLE mpp21090_drop_lastcol_index_dml_timestamptz(
col1 int,
col2 decimal,
col3 char,
col4 date,
col5 timestamptz
) DISTRIBUTED by(col3);

DROP INDEX IF EXISTS mpp21090_drop_lastcol_index_dml_idx_timestamptz;
CREATE INDEX mpp21090_drop_lastcol_index_dml_idx_timestamptz on mpp21090_drop_lastcol_index_dml_timestamptz(col3);

INSERT INTO mpp21090_drop_lastcol_index_dml_timestamptz VALUES(0,0.00,'a','2014-01-01','2013-12-30 12:00:00 PST');
SELECT * FROM mpp21090_drop_lastcol_index_dml_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_drop_lastcol_index_dml_timestamptz DROP COLUMN col5;

INSERT INTO mpp21090_drop_lastcol_index_dml_timestamptz SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_lastcol_index_dml_timestamptz ORDER BY 1,2,3,4;

UPDATE mpp21090_drop_lastcol_index_dml_timestamptz SET col3='c' WHERE col3 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_lastcol_index_dml_timestamptz ORDER BY 1,2,3,4;

DELETE FROM mpp21090_drop_lastcol_index_dml_timestamptz WHERE col3='c';
SELECT * FROM mpp21090_drop_lastcol_index_dml_timestamptz ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_boolean;
CREATE TABLE mpp21090_drop_midcol_dml_boolean
(
col1 int,
col2 decimal,
col3 boolean,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_boolean VALUES(0,0.00,True,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_boolean ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_boolean DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_boolean SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_boolean ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_boolean SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_boolean ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_boolean;
CREATE TABLE mpp21090_drop_midcol_dml_boolean
(
col1 int,
col2 decimal,
col3 boolean,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_boolean VALUES(0,0.00,True,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_boolean ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_boolean DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_boolean SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_boolean ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_boolean SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_boolean ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_boolean;
CREATE TABLE mpp21090_drop_midcol_dml_boolean
(
col1 int,
col2 decimal,
col3 boolean,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_boolean VALUES(0,0.00,True,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_boolean ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_boolean DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_boolean SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_boolean ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_boolean SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_boolean ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_char;
CREATE TABLE mpp21090_drop_midcol_dml_char
(
col1 int,
col2 decimal,
col3 char,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_char VALUES(0,0.00,'z','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_char ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_char DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_char SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_char ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_char SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_char ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_char;
CREATE TABLE mpp21090_drop_midcol_dml_char
(
col1 int,
col2 decimal,
col3 char,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_char VALUES(0,0.00,'z','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_char ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_char DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_char SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_char ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_char SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_char ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_char;
CREATE TABLE mpp21090_drop_midcol_dml_char
(
col1 int,
col2 decimal,
col3 char,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_char VALUES(0,0.00,'z','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_char ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_char DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_char SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_char ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_char SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_char ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_date;
CREATE TABLE mpp21090_drop_midcol_dml_date
(
col1 int,
col2 decimal,
col3 date,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_date VALUES(0,0.00,'2013-12-31','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_date ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_date DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_date SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_date ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_date SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_date ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_date;
CREATE TABLE mpp21090_drop_midcol_dml_date
(
col1 int,
col2 decimal,
col3 date,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_date VALUES(0,0.00,'2013-12-31','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_date ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_date DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_date SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_date ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_date SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_date ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_date;
CREATE TABLE mpp21090_drop_midcol_dml_date
(
col1 int,
col2 decimal,
col3 date,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_date VALUES(0,0.00,'2013-12-31','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_date ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_date DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_date SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_date ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_date SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_date ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_decimal;
CREATE TABLE mpp21090_drop_midcol_dml_decimal
(
col1 int,
col2 decimal,
col3 decimal,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_decimal VALUES(0,0.00,2.00,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_decimal ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_decimal DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_decimal SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_decimal ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_decimal SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_decimal ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_decimal;
CREATE TABLE mpp21090_drop_midcol_dml_decimal
(
col1 int,
col2 decimal,
col3 decimal,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_decimal VALUES(0,0.00,2.00,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_decimal ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_decimal DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_decimal SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_decimal ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_decimal SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_decimal ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_decimal;
CREATE TABLE mpp21090_drop_midcol_dml_decimal
(
col1 int,
col2 decimal,
col3 decimal,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_decimal VALUES(0,0.00,2.00,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_decimal ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_decimal DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_decimal SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_decimal ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_decimal SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_decimal ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_float;
CREATE TABLE mpp21090_drop_midcol_dml_float
(
col1 int,
col2 decimal,
col3 float,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_float VALUES(0,0.00,2.00,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_float ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_float DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_float SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_float ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_float SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_float ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_float;
CREATE TABLE mpp21090_drop_midcol_dml_float
(
col1 int,
col2 decimal,
col3 float,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_float VALUES(0,0.00,2.00,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_float ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_float DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_float SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_float ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_float SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_float ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_float;
CREATE TABLE mpp21090_drop_midcol_dml_float
(
col1 int,
col2 decimal,
col3 float,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_float VALUES(0,0.00,2.00,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_float ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_float DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_float SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_float ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_float SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_float ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_int4;
CREATE TABLE mpp21090_drop_midcol_dml_int4
(
col1 int,
col2 decimal,
col3 int4,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_int4 VALUES(0,0.00,2000000000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_int4 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_int4 DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_int4 SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_int4 ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_int4 SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_int4 ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_int4;
CREATE TABLE mpp21090_drop_midcol_dml_int4
(
col1 int,
col2 decimal,
col3 int4,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_int4 VALUES(0,0.00,2000000000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_int4 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_int4 DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_int4 SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_int4 ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_int4 SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_int4 ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_int4;
CREATE TABLE mpp21090_drop_midcol_dml_int4
(
col1 int,
col2 decimal,
col3 int4,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_int4 VALUES(0,0.00,2000000000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_int4 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_int4 DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_int4 SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_int4 ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_int4 SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_int4 ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_int8;
CREATE TABLE mpp21090_drop_midcol_dml_int8
(
col1 int,
col2 decimal,
col3 int8,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_int8 VALUES(0,0.00,2000000000000000000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_int8 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_int8 DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_int8 SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_int8 ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_int8 SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_int8 ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_int8;
CREATE TABLE mpp21090_drop_midcol_dml_int8
(
col1 int,
col2 decimal,
col3 int8,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_int8 VALUES(0,0.00,2000000000000000000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_int8 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_int8 DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_int8 SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_int8 ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_int8 SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_int8 ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_int8;
CREATE TABLE mpp21090_drop_midcol_dml_int8
(
col1 int,
col2 decimal,
col3 int8,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_int8 VALUES(0,0.00,2000000000000000000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_int8 ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_int8 DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_int8 SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_int8 ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_int8 SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_int8 ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_int;
CREATE TABLE mpp21090_drop_midcol_dml_int
(
col1 int,
col2 decimal,
col3 int,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_int VALUES(0,0.00,20000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_int DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_int SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_int SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_int;
CREATE TABLE mpp21090_drop_midcol_dml_int
(
col1 int,
col2 decimal,
col3 int,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_int VALUES(0,0.00,20000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_int DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_int SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_int SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_int;
CREATE TABLE mpp21090_drop_midcol_dml_int
(
col1 int,
col2 decimal,
col3 int,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_int VALUES(0,0.00,20000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_int DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_int SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_int SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_int ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_interval;
CREATE TABLE mpp21090_drop_midcol_dml_interval
(
col1 int,
col2 decimal,
col3 interval,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_interval VALUES(0,0.00,'1 day','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_interval ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_interval DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_interval SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_interval ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_interval SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_interval ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_interval;
CREATE TABLE mpp21090_drop_midcol_dml_interval
(
col1 int,
col2 decimal,
col3 interval,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_interval VALUES(0,0.00,'1 day','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_interval ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_interval DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_interval SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_interval ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_interval SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_interval ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_interval;
CREATE TABLE mpp21090_drop_midcol_dml_interval
(
col1 int,
col2 decimal,
col3 interval,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_interval VALUES(0,0.00,'1 day','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_interval ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_interval DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_interval SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_interval ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_interval SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_interval ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_numeric;
CREATE TABLE mpp21090_drop_midcol_dml_numeric
(
col1 int,
col2 decimal,
col3 numeric,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_numeric VALUES(0,0.00,2.000000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_numeric ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_numeric DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_numeric SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_numeric ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_numeric SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_numeric ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_numeric;
CREATE TABLE mpp21090_drop_midcol_dml_numeric
(
col1 int,
col2 decimal,
col3 numeric,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_numeric VALUES(0,0.00,2.000000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_numeric ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_numeric DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_numeric SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_numeric ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_numeric SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_numeric ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_numeric;
CREATE TABLE mpp21090_drop_midcol_dml_numeric
(
col1 int,
col2 decimal,
col3 numeric,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_numeric VALUES(0,0.00,2.000000,'a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_numeric ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_numeric DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_numeric SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_numeric ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_numeric SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_numeric ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_text;
CREATE TABLE mpp21090_drop_midcol_dml_text
(
col1 int,
col2 decimal,
col3 text,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_text VALUES(0,0.00,'abcdefghijklmnop','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_text ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_text DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_text SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_text ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_text SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_text ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_text;
CREATE TABLE mpp21090_drop_midcol_dml_text
(
col1 int,
col2 decimal,
col3 text,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_text VALUES(0,0.00,'abcdefghijklmnop','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_text ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_text DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_text SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_text ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_text SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_text ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_text;
CREATE TABLE mpp21090_drop_midcol_dml_text
(
col1 int,
col2 decimal,
col3 text,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_text VALUES(0,0.00,'abcdefghijklmnop','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_text ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_text DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_text SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_text ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_text SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_text ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_time;
CREATE TABLE mpp21090_drop_midcol_dml_time
(
col1 int,
col2 decimal,
col3 time,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_time VALUES(0,0.00,'12:00:00','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_time ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_time DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_time SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_time ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_time SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_time ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_time;
CREATE TABLE mpp21090_drop_midcol_dml_time
(
col1 int,
col2 decimal,
col3 time,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_time VALUES(0,0.00,'12:00:00','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_time ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_time DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_time SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_time ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_time SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_time ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_time;
CREATE TABLE mpp21090_drop_midcol_dml_time
(
col1 int,
col2 decimal,
col3 time,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_time VALUES(0,0.00,'12:00:00','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_time ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_time DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_time SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_time ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_time SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_time ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_timestamp;
CREATE TABLE mpp21090_drop_midcol_dml_timestamp
(
col1 int,
col2 decimal,
col3 timestamp,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_timestamp VALUES(0,0.00,'2013-12-31 12:00:00','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_timestamp ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_timestamp DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_timestamp SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_timestamp ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_timestamp SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_timestamp ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_timestamp;
CREATE TABLE mpp21090_drop_midcol_dml_timestamp
(
col1 int,
col2 decimal,
col3 timestamp,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_timestamp VALUES(0,0.00,'2013-12-31 12:00:00','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_timestamp ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_timestamp DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_timestamp SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_timestamp ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_timestamp SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_timestamp ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_timestamp;
CREATE TABLE mpp21090_drop_midcol_dml_timestamp
(
col1 int,
col2 decimal,
col3 timestamp,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_timestamp VALUES(0,0.00,'2013-12-31 12:00:00','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_timestamp ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_timestamp DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_timestamp SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_timestamp ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_timestamp SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_timestamp ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_timestamptz;
CREATE TABLE mpp21090_drop_midcol_dml_timestamptz
(
col1 int,
col2 decimal,
col3 timestamptz,
col4 char,
col5 date
) with (appendonly= true)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_timestamptz VALUES(0,0.00,'2013-12-31 12:00:00 PST','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_timestamptz ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_timestamptz DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_timestamptz SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_timestamptz ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_timestamptz SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_timestamptz ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_timestamptz;
CREATE TABLE mpp21090_drop_midcol_dml_timestamptz
(
col1 int,
col2 decimal,
col3 timestamptz,
col4 char,
col5 date
) with (appendonly= true, orientation= column)  DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_timestamptz VALUES(0,0.00,'2013-12-31 12:00:00 PST','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_timestamptz ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_timestamptz DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_timestamptz SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_timestamptz ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_timestamptz SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_timestamptz ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_midcol_dml_timestamptz;
CREATE TABLE mpp21090_drop_midcol_dml_timestamptz
(
col1 int,
col2 decimal,
col3 timestamptz,
col4 char,
col5 date
) DISTRIBUTED by(col4);
INSERT INTO mpp21090_drop_midcol_dml_timestamptz VALUES(0,0.00,'2013-12-31 12:00:00 PST','a','2014-01-01');
SELECT * FROM mpp21090_drop_midcol_dml_timestamptz ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_midcol_dml_timestamptz DROP COLUMN col3;
INSERT INTO mpp21090_drop_midcol_dml_timestamptz SELECT 1,1.00,'b','2014-01-02';
SELECT * FROM mpp21090_drop_midcol_dml_timestamptz ORDER BY 1,2,3,4;
UPDATE mpp21090_drop_midcol_dml_timestamptz SET col4='c' WHERE col4 = 'b' AND col1 = 1;
SELECT * FROM mpp21090_drop_midcol_dml_timestamptz ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_boolean;
CREATE TABLE mpp21090_drop_multicol_dml_boolean(
col1 boolean,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_boolean VALUES(True,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_boolean DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_boolean DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_boolean SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_boolean SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_boolean WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_boolean;
CREATE TABLE mpp21090_drop_multicol_dml_boolean(
col1 boolean,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_boolean VALUES(True,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_boolean DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_boolean DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_boolean SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_boolean SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_boolean WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_boolean;
CREATE TABLE mpp21090_drop_multicol_dml_boolean(
col1 boolean,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_boolean VALUES(True,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_boolean DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_boolean DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_boolean SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_boolean SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_boolean WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_boolean ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_char;
CREATE TABLE mpp21090_drop_multicol_dml_char(
col1 char,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_char VALUES('z',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_char DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_char DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_char SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_char SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_char WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_char;
CREATE TABLE mpp21090_drop_multicol_dml_char(
col1 char,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_char VALUES('z',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_char DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_char DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_char SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_char SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_char WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_multicol_dml_char;
CREATE TABLE mpp21090_drop_multicol_dml_char(
col1 char,
col2 decimal,
col3 char,
col4 date,
col5 int
)  DISTRIBUTED by (col3);
INSERT INTO mpp21090_drop_multicol_dml_char VALUES('z',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_multicol_dml_char DROP COLUMN col1;
ALTER TABLE mpp21090_drop_multicol_dml_char DROP COLUMN col4;
INSERT INTO mpp21090_drop_multicol_dml_char SELECT 1.00,'b',1;
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3;
UPDATE mpp21090_drop_multicol_dml_char SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_multicol_dml_char WHERE col3='c';
SELECT * FROM mpp21090_drop_multicol_dml_char ORDER BY 1,2,3;


-- start_ignore
drop schema sql_partition_201_250 cascade;
-- end_ignore
