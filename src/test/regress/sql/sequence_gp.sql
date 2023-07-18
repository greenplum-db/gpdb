--
-- Test Overflow with NO CYCLE
--
CREATE TABLE tmp_table (a int);
INSERT INTO tmp_table VALUES (0),(1),(2),(3);

-- Test execution of nextval on coordinator with CACHE 1
CREATE SEQUENCE tmp_seq INCREMENT 1 MINVALUE 1 MAXVALUE 2 START 1 CACHE 1 NO CYCLE;
SELECT nextval('tmp_seq');
SELECT nextval('tmp_seq');
-- Fails because it reaches MAXVALUE
SELECT nextval('tmp_seq');
DROP SEQUENCE tmp_seq;

-- Test that ORCA and Planner return the same results although they produce different execution plans.
CREATE SEQUENCE tmp_seq INCREMENT 1 MINVALUE 1 MAXVALUE 4 START 1 CACHE 1 NO CYCLE;
SELECT val from (SELECT nextval('tmp_seq'), a as val FROM tmp_table ORDER BY a) as val ORDER BY val;
DROP SEQUENCE tmp_seq;

-- Test execution of nextval on coordinator with CACHE > 1
CREATE SEQUENCE tmp_seq INCREMENT 1 MINVALUE 1 MAXVALUE 2 START 1 CACHE 20 NO CYCLE;
SELECT nextval('tmp_seq');
SELECT nextval('tmp_seq');
-- Fails because it reaches MAXVALUE
SELECT nextval('tmp_seq');
DROP SEQUENCE tmp_seq;

-- Test execution of nextval on coordinator (when optimizer = on) and segments (when optimizer=off) with CACHE > 1
CREATE TABLE tmp_table_cache (a int, b int);
-- forcing the values to go on one segment only, to predict the sequence values.
INSERT INTO tmp_table_cache VALUES (1,0),(1,1),(1,2),(1,3);
CREATE SEQUENCE tmp_seq INCREMENT 1 MINVALUE 1 MAXVALUE 4 START 1 CACHE 3 NO CYCLE;
SELECT nextval('tmp_seq'), * FROM tmp_table_cache;
SELECT * from tmp_seq;
SELECT seqrelid::regclass, seqtypid::regtype, seqstart, seqincrement, seqmax, seqmin, seqcache, seqcycle FROM pg_sequence WHERE seqrelid='tmp_seq'::regclass;
DROP SEQUENCE tmp_seq;

CREATE SEQUENCE tmp_seq INCREMENT 1 MINVALUE 1 MAXVALUE 3 START 1 CACHE 3 NO CYCLE;
SELECT nextval('tmp_seq'), a FROM tmp_table ORDER BY a;
DROP SEQUENCE tmp_seq;

DROP TABLE tmp_table;

CREATE SEQUENCE mytable_gid_seq CACHE 1;
CREATE TABLE mytable (size INTEGER, gid BIGINT NOT NULL DEFAULT nextval('mytable_gid_seq'));
ALTER SEQUENCE mytable_gid_seq RESTART WITH 9223372036854775805;
/* Consume rest of serial sequence column values */
INSERT INTO mytable VALUES (1), (2), (3);
SELECT gid FROM mytable;
INSERT INTO mytable VALUES(4);
SELECT gid FROM mytable;
INSERT INTO mytable SELECT * FROM generate_series(1, 10)i;
SELECT gid FROM mytable;
SELECT * FROM mytable_gid_seq;
SELECT seqrelid::regclass, seqtypid::regtype, seqstart, seqincrement, seqmax, seqmin, seqcache, seqcycle FROM pg_sequence WHERE seqrelid='mytable_gid_seq'::regclass;

CREATE TABLE out_of_range_insert (size INTEGER, gid serial NOT NULL);
ALTER SEQUENCE out_of_range_insert_gid_seq RESTART WITH 2147483646;
INSERT INTO out_of_range_insert VALUES (1), (2), (3);
SELECT * FROM out_of_range_insert ORDER BY gid;
SELECT * FROM out_of_range_insert_gid_seq;
SELECT seqrelid::regclass, seqtypid::regtype, seqstart, seqincrement, seqmax, seqmin, seqcache, seqcycle FROM pg_sequence WHERE seqrelid='out_of_range_insert_gid_seq'::regclass;

CREATE SEQUENCE descending_sequence INCREMENT -1 MINVALUE 1 MAXVALUE 9223372036854775806 START 9223372036854775806 CACHE 1;
SELECT nextval('descending_sequence');
CREATE TABLE descending_sequence_insert(a bigint, b bigint);
INSERT INTO descending_sequence_insert SELECT i, nextval('descending_sequence') FROM generate_series(1, 10)i;
SELECT * FROM descending_sequence_insert ORDER BY b DESC;
SELECT * FROM descending_sequence;
SELECT seqrelid::regclass, seqtypid::regtype, seqstart, seqincrement, seqmax, seqmin, seqcache, seqcycle FROM pg_sequence WHERE seqrelid='descending_sequence'::regclass;

-- Test that we don't produce duplicate sequence values
DROP SEQUENCE IF EXISTS check_no_duplicates;
CREATE SEQUENCE check_no_duplicates;
SELECT nextval('check_no_duplicates') FROM gp_dist_random('gp_id');
SELECT nextval('check_no_duplicates');
SELECT nextval('check_no_duplicates') FROM gp_dist_random('gp_id');
