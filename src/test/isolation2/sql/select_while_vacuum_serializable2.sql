-- @Description Ensures that a serializable select before during a vacuum operation avoids the compaction.
-- 
DROP TABLE IF EXISTS ao;
CREATE TABLE ao (a INT) WITH (appendonly=true);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);
insert into ao select generate_series(1,1000);

DELETE FROM ao WHERE a < 128;
1: BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
1: SELECT COUNT(*) FROM ao;
2U: SELECT segno, tupcount FROM gp_toolkit.__gp_aoseg_name('ao');
2: VACUUM ao;
1: SELECT COUNT(*) FROM ao;
1: COMMIT;
3: INSERT INTO ao VALUES (0);
2U: SELECT segno, tupcount FROM gp_toolkit.__gp_aoseg_name('ao');
