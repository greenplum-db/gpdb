-- @Description Tests the compaction of data inserted in utility mode
-- 

INSERT INTO foo VALUES (1, 1, 'c');
SELECT segno, tupcount, state FROM gp_aoseg_name('foo');
2U: INSERT INTO foo VALUES (2, 2, 'c');
2U: INSERT INTO foo VALUES (3, 3, 'c');
2U: SELECT segno, tupcount, state FROM gp_aoseg_name('foo');
-- We know that the master does update its tupcount yet
SELECT segno, tupcount, state FROM gp_aoseg_name('foo');
DELETE FROM foo WHERE a = 2;
UPDATE foo SET b = -1 WHERE a = 3;
VACUUM foo;
2U: SELECT segno, tupcount, state FROM gp_aoseg_name('foo');
SELECT segno, tupcount, state FROM gp_aoseg_name('foo');
