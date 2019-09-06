DROP TABLE IF EXISTS tsudf;

CREATE TABLE tsudf (c int, d int);

CREATE OR REPLACE FUNCTION func1(int) RETURNS int AS $$
BEGIN
  UPDATE tsudf SET d = d+1 WHERE c = $1; /* in func */
  RETURN $1; /* in func */
END; /* in func */
$$
LANGUAGE plpgsql;

INSERT INTO tsudf select i, i+1 from generate_series(1,10) i;

SELECT gp_inject_fault('upgrade_row_lock', 'reset', 1);
SELECT gp_inject_fault('upgrade_row_lock', 'sleep', '', '', '', 1, -1, 10, 1);

3&: SELECT * FROM func1(1);
4: SELECT * FROM func1(2);

3<:
3q:
4q:

SELECT gp_inject_fault('upgrade_row_lock', 'reset', 1);
