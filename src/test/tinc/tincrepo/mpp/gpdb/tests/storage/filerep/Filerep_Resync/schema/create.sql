-- 
-- @created 2013-03-29 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags storage
-- @product_version gpdb: 4.2, 4.3
-- @description Transaction on AO table

DROP TABLE ta;

CREATE TABLE ta(a int) WITH(appendonly=true)
