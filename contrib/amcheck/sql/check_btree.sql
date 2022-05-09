-- minimal test, basically just verifying that amcheck
CREATE TABLE bttest_a(id int8);
CREATE TABLE bttest_b(id int8);

INSERT INTO bttest_a SELECT * FROM generate_series(1, 100000);
INSERT INTO bttest_b SELECT * FROM generate_series(100000, 1, -1);

CREATE INDEX bttest_a_idx ON bttest_a USING btree (id);
CREATE INDEX bttest_b_idx ON bttest_b USING btree (id);

CREATE ROLE regress_bttest_role;

-- verify permissions are checked (error due to function not callable)
SET ROLE regress_bttest_role;
SELECT bt_index_check('bttest_a_idx'::regclass);
SELECT bt_index_parent_check('bttest_a_idx'::regclass);
RESET ROLE;

-- we, intentionally, don't check relation permissions - it's useful
-- to run this cluster-wide with a restricted account, and as tested
-- above explicit permission has to be granted for that.
GRANT EXECUTE ON FUNCTION bt_index_check(regclass) TO regress_bttest_role;
GRANT EXECUTE ON FUNCTION bt_index_parent_check(regclass) TO regress_bttest_role;
SET ROLE regress_bttest_role;
SELECT bt_index_check('bttest_a_idx');
SELECT bt_index_parent_check('bttest_a_idx');
RESET ROLE;

-- verify plain tables are rejected (error)
SELECT bt_index_check('bttest_a');
SELECT bt_index_parent_check('bttest_a');

-- verify non-existing indexes are rejected (error)
SELECT bt_index_check(17);
SELECT bt_index_parent_check(17);

-- verify wrong index types are rejected (error)
BEGIN;
CREATE INDEX bttest_a_bitmap_idx ON bttest_a USING bitmap(id);
SELECT bt_index_parent_check('bttest_a_bitmap_idx');
ROLLBACK;

-- normal check outside of xact
SELECT bt_index_check('bttest_a_idx');
-- more expansive test
SELECT bt_index_parent_check('bttest_b_idx');

BEGIN;
SELECT bt_index_check('bttest_a_idx');
SELECT bt_index_parent_check('bttest_b_idx');
-- GPDB
SELECT bt_index_check_on_all('bttest_a_idx');
SELECT bt_index_parent_check_on_all('bttest_b_idx');
-- make sure we don't have any leftover locks
SELECT * FROM pg_locks
WHERE relation = ANY(ARRAY['bttest_a', 'bttest_a_idx', 'bttest_b', 'bttest_b_idx']::regclass[])
    AND pid = pg_backend_pid();
COMMIT;

--
-- Check that index expressions and predicates are run as the table's owner
--
TRUNCATE bttest_a;
INSERT INTO bttest_a SELECT * FROM generate_series(1, 1000);
ALTER TABLE bttest_a OWNER TO regress_bttest_role;
-- A dummy index function checking current_user
CREATE FUNCTION ifun(int8) RETURNS int8 AS $$
BEGIN
	ASSERT current_user = 'regress_bttest_role',
		format('ifun(%s) called by %s', $1, current_user);
	RETURN $1;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

CREATE INDEX bttest_a_expr_idx ON bttest_a ((ifun(id) + ifun(0)))
	WHERE ifun(id + 10) > ifun(10);

SELECT bt_index_check('bttest_a_expr_idx');

-- cleanup
DROP TABLE bttest_a;
DROP TABLE bttest_b;
DROP FUNCTION ifun(int8);
DROP OWNED BY regress_bttest_role; -- permissions
DROP ROLE regress_bttest_role;

-- GPDB: test the AO and AOCS tables
-- start_matchsubs
-- m/calculated checksum \d+ but expected \d+/
-- s/calculated checksum \d+ but expected \d+/calculated checksum XXXX but expected XXXX/
-- end_matchsubs

-- start_matchsubs
-- m/^ERROR:  invalid page in block 0 of relation .*$/
-- s/^ERROR:  invalid page in block 0 of relation .*$/ERROR:  invalid page in block 0 of relation/
-- end_matchsubs

-- start_matchignore
-- m/^INFO:  corrupting file/
-- m/^INFO:  skipping non-existent file/
-- end_matchignore


-- start_ignore
CREATE LANGUAGE plpythonu;
-- end_ignore
CREATE FUNCTION get_index_path(tbl regclass) returns text as $$
  (select 'base/' || db.oid || '/' || c.relfilenode from pg_class c, pg_database db where c.oid = $1 AND db.datname = current_database())
$$ language sql VOLATILE;

-- Copy this function from src/test/regress/sql/ao_checksum_corruption.sql
-- Corrupt data file at given path (if it exists on this segment)
--
-- If corruption_offset is negative, it's an offset from the end of file.
-- Otherwise it's from the beginning of file.
--
-- Returns 0. (That's handy in the way this function is called, because we can
-- do a SUM() over the return values, and it's always 0, regardless of the
-- number of segemnts in the cluster.)
CREATE FUNCTION corrupt_file(data_file text, corruption_offset int4)
RETURNS integer as $$
  import os;

  if not os.path.isfile(data_file):
    plpy.info('skipping non-existent file %s' % (data_file))
  else:
    plpy.info('corrupting file %s at %s' % (data_file, corruption_offset))

    with open(data_file , "rb+") as f:
      char_location=0
      write_char='*' # CONST.CORRUPTION

      if corruption_offset >= 0:
        f.seek(corruption_offset, 0)
      else:
        f.seek(corruption_offset, 2)

      f.write(write_char)
      f.close()

  return 0
$$ LANGUAGE plpythonu;

-- check index on AO table
CREATE TABLE bttest_a_ao(id int8) WITH (appendonly=true);
CREATE TABLE bttest_b_ao(id int8) WITH (appendonly=true);
INSERT INTO bttest_a_ao SELECT * FROM generate_series(1, 100000);
INSERT INTO bttest_b_ao SELECT * FROM generate_series(100000, 1, -1);
CREATE INDEX bttest_a_ao_idx ON bttest_a_ao USING btree (id);
CREATE INDEX bttest_b_ao_idx ON bttest_b_ao USING btree (id);

SELECT bt_index_check_on_all('bttest_a_ao_idx');
SELECT bt_index_parent_check_on_all('bttest_b_ao_idx');

-- corrupt the file
SELECT corrupt_file(get_index_path('bttest_a_ao_idx'), 0) from gp_dist_random('gp_id') where gp_segment_id = 0;
SELECT bt_index_check_on_all('bttest_a_ao_idx');
select corrupt_file(get_index_path('bttest_b_ao_idx'), 0) from gp_dist_random('gp_id') where gp_segment_id = 0;
SELECT bt_index_parent_check_on_all('bttest_b_ao_idx');

DROP TABLE bttest_a_ao;
DROP TABLE bttest_b_ao;

-- check index on AOCS table
CREATE TABLE bttest_a_aocs(id int8) WITH (appendonly=true, orientation=column);
CREATE TABLE bttest_b_aocs(id int8) WITH (appendonly=true, orientation=column);
INSERT INTO bttest_a_aocs SELECT * FROM generate_series(1, 100000);
INSERT INTO bttest_b_aocs SELECT * FROM generate_series(100000, 1, -1);
CREATE INDEX bttest_a_aocs_idx ON bttest_a_aocs USING btree (id);
CREATE INDEX bttest_b_aocs_idx ON bttest_b_aocs USING btree (id);

SELECT bt_index_check_on_all('bttest_a_aocs_idx');
SELECT bt_index_parent_check_on_all('bttest_b_aocs_idx');

-- corrupt the file
SELECT corrupt_file(get_index_path('bttest_a_aocs_idx'), 0) from gp_dist_random('gp_id') where gp_segment_id = 0;
SELECT bt_index_check_on_all('bttest_a_aocs_idx');
select corrupt_file(get_index_path('bttest_b_aocs_idx'), 0) from gp_dist_random('gp_id') where gp_segment_id = 0;
SELECT bt_index_parent_check_on_all('bttest_b_aocs_idx');

DROP TABLE bttest_a_aocs;
DROP TABLE bttest_b_aocs;
