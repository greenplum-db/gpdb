-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TEMPORARY TABLE on_commit1(a int, b text)
  ON COMMIT DELETE ROWS;

INSERT INTO on_commit1 VALUES(1, 'foo'), (2, 'bar');
INSERT INTO on_commit1 SELECT i, NULL FROM generate_series(1, 100)i;
CREATE VIEW v AS SELECT * FROM on_commit1;
DROP VIEW v;

INSERT INTO on_commit1 VALUES(1, 'foo'), (2, 'bar');
INSERT INTO on_commit1 SELECT i, NULL FROM generate_series(1, 100)i;
CREATE VIEW v AS SELECT * FROM on_commit1;
DROP VIEW v;

INSERT INTO on_commit1 VALUES(1, 'foo'), (2, 'bar');
INSERT INTO on_commit1 SELECT i, NULL FROM generate_series(1, 100)i;
CREATE VIEW v AS SELECT * FROM on_commit1;
DROP VIEW v;

INSERT INTO on_commit1 VALUES(1, 'foo'), (2, 'bar');
INSERT INTO on_commit1 SELECT i, NULL FROM generate_series(1, 100)i;
CREATE VIEW v AS SELECT * FROM on_commit1;
DROP VIEW v;

