-- @author prabhd
-- @created 2012-02-01 12:00:00
-- @modified 2013-02-01 12:00:00
-- @tags cte HAWQ
-- @description MPP-19696

WITH v1 AS (SELECT b FROM r), v2 as (SELECT b FROM v1) SELECT * FROM v2 WHERE b < 5 ORDER BY 1;
