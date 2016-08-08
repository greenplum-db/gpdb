-- @Description Tests basic update commands on ao tables when aborted
-- 

BEGIN;
UPDATE foo SET b=42 WHERE a < 10;
SELECT COUNT(b) FROM foo WHERE b = 42;
SELECT COUNT(b) FROM foo WHERE b < 10;
ROLLBACK;
SELECT COUNT(b) FROM foo WHERE b = 42;
SELECT COUNT(b) FROM foo WHERE b < 10;
set gp_select_invisible=true;
SELECT COUNT(b) FROM foo WHERE b = 42;
SELECT COUNT(b) FROM foo WHERE b < 10;
