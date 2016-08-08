-- 
-- @created 2013-03-14 12:00:00
-- @modified 2012-03-14 12:00:00
-- @tags storage
-- @product_version gpdb: 4.2, 4.3
-- @description Transaction on AO table

BEGIN;

INSERT INTO ta 
VALUES(0),(1);

-- Sleep introduced so that all the mirror segments gets
-- killed before aborting
SELECT pg_sleep(300);

ABORT;

INSERT INTO ta
VALUES(0),(1);


