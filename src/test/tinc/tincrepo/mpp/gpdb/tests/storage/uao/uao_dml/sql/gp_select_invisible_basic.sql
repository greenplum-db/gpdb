-- @Description Tests that gp_select_invisible is handled correctly with row append-only.
-- 

SELECT COUNT(*) FROM foo;
SET gp_select_invisible=true;
SELECT COUNT(*) FROM foo;
