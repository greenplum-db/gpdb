-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 text,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a','abcdefghijklmnop',1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4='qrstuvwxyz' WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










