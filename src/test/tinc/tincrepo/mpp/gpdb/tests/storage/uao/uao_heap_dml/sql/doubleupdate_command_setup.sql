-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS bar;

CREATE TABLE foo (a INT, b INT) ;
INSERT INTO foo VALUES (10, 10);
CREATE TABLE bar (a INT, b INT) ;
INSERT INTO bar VALUES (15, 10);
INSERT INTO bar VALUES (20, 10);
