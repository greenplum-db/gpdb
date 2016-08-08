-- @Description Tests the basic behavior of (lazy) vacuum when called from utility mode
-- 

DELETE FROM foo WHERE a < 20;
SELECT COUNT(*) FROM foo;
2U: VACUUM foo;
SELECT COUNT(*) FROM foo;
2U: SELECT segno, tupcount FROM gp_aoseg_name('foo');
