-- @Description Tests delete on compressed ao tables
-- 

INSERT INTO FOO VALUES (1, 1, 'c');
SELECT * FROM foo;
DELETE FROM foo;
SELECT * FROM foo;
