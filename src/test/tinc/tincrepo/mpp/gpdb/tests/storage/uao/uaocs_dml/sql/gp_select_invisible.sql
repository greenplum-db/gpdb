-- @Description Tests that gp_select_invisible is handled correctly with UAO
-- 

SELECT COUNT(*) FROM foo WHERE a < 4;
DELETE FROM foo WHERE a < 4;
SELECT COUNT(*) FROM foo WHERE a < 4;
SET gp_select_invisible=true;
-- Should display all tuples
SELECT COUNT(*) FROM foo WHERE a < 4;
-- Now an interesting case where we update with gp_select_invisible active
UPDATE foo SET b = 0 WHERE a < 5;
-- We should not re-activate the deleted tuples
SET gp_select_invisible=false;
SELECT COUNT(*) FROM foo WHERE b = 0;
-- Now the same re-activation test with update
SET gp_select_invisible=true;
UPDATE foo SET b = -1 WHERE a = 4;
SET gp_select_invisible=false;
SELECT COUNT(*) FROM foo WHERE a = 4;
