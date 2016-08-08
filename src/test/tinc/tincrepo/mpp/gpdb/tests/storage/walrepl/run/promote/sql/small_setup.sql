-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo(a int, b int);
INSERT INTO foo VALUES(1, 10),(2, 20);

DROP FUNCTION myfunc(int);
CREATE FUNCTION myfunc(int) RETURNS int AS $$
DECLARE
BEGIN
  RETURN 1 + $1;
END;
$$ LANGUAGE plpgsql;
