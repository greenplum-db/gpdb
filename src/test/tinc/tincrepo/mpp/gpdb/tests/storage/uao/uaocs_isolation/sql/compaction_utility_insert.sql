-- @Description Tests the compaction of data inserted in utility mode
-- 

INSERT INTO foo VALUES (1, 1, 'c');
SELECT DISTINCT segno, tupcount, state FROM gp_aocsseg_name('foo');
2U: INSERT INTO foo VALUES (2, 2, 'c');
2U: INSERT INTO foo VALUES (3, 3, 'c');
2U: SELECT DISTINCT segno, tupcount, state FROM gp_aocsseg_name('foo');
-- We know that the master does update its tupcount yet
SELECT DISTINCT segno, tupcount, state FROM gp_aocsseg_name('foo');
DELETE FROM foo WHERE a = 2;
UPDATE foo SET b = -1 WHERE a = 3;
VACUUM foo;
2U: SELECT DISTINCT segno, tupcount, state FROM gp_aocsseg_name('foo');
SELECT DISTINCT segno, tupcount, state FROM gp_aocsseg_name('foo');
