-- @Description Tests that deleting the same tuple twice within the same
-- commands works fine.
-- 

SELECT * FROM foo;
UPDATE foo SET b = bar.a FROM bar WHERE foo.b = bar.b;
set gp_select_invisible = false;
SELECT count(*) as only_visible FROM foo;
set gp_select_invisible = true;
SELECT count(*) as visible_and_invisible FROM foo;
set gp_select_invisible = false;

