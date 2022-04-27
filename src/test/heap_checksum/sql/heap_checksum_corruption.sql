-- The following test simulates corruption of a heap relation file and
-- verifies the select behavior on this corrupted table, the related indexes
-- and toast tables.

-- Mask out the expected and actual values of the checksums when comparing the
-- result. All we care about is that they don't match.
--
-- start_matchsubs
-- m/^ERROR:.*invalid page in block.*/
-- s/invalid page in block.*/invalid page in block/
-- m/^ERROR:  missing chunk number.*/
-- s/missing chunk number.*/missing chunk number/
-- end_matchsubs

-- Ignore the status messages from the helper function. They're useful for
-- debugging, but the output is different on every invocation.
-- start_matchignore
-- m/^INFO:  corrupting file/
-- m/^INFO:  skipping non-existent file/
-- m/^WARNING:  page verification failed, calculated checksum.*/
-- end_matchignore

-- start_ignore
CREATE LANGUAGE plpython3u;
-- end_ignore

-- Create our test tables (and functions) in a bespoken schema that we can drop
-- at the end. We don't want to leave any corrupt files lying around!
CREATE SCHEMA corrupt_heap_checksum;
set search_path='corrupt_heap_checksum',public;

-- to ignore the CONTEXT from messages from the plpython helpers, and to ignore
-- DETAILs from the checksum errors.
\set VERBOSITY terse

-- Return path to the file holding data for the given table (relative to
-- $PGDATA).
--
CREATE FUNCTION get_relation_path(tbl regclass) returns text as $$
  (select 'base/' || db.oid || '/' || c.relfilenode from pg_class c, pg_database db where c.oid = $1 AND db.datname = current_database())
$$ language sql VOLATILE;

-- Return path to the file holding data for the given table's TOAST table (relative to
-- $PGDATA).
--
CREATE FUNCTION get_toast_path(tbl regclass) returns text as $$
  (select 'base/' || db.oid || '/' || c.relfilenode from pg_class c, pg_database db where c.oid = ( SELECT reltoastrelid FROM pg_class  WHERE oid = $1) AND db.datname = current_database())
$$ language sql VOLATILE;

-- Return name of the given table's TOAST table (relative to
-- $PGDATA).
--
CREATE FUNCTION get_toast_name(tbl regclass) returns text as $$
  (select relname::text from pg_class where oid = ( SELECT reltoastrelid FROM pg_class  WHERE oid = $1))
$$ language sql VOLATILE;

-- Return path to the file holding data for the given table's index (relative to
-- $PGDATA).
--
CREATE FUNCTION get_index_path(tbl regclass) returns text as $$
  (select 'base/' || db.oid || '/' || c.relfilenode from pg_class c, pg_database db where c.oid = ( SELECT indexrelid FROM pg_index  WHERE indrelid = $1) AND db.datname = current_database())
$$ language sql VOLATILE;

-- Corrupt data file at given path (if it exists on this segment)
--
-- If corruption_offset is negative, it's an offset from the end of file.
-- Otherwise it's from the beginning of file.
--
-- Returns 0. (That's handy in the way this function is called, because we can
-- do a SUM() over the return values, and it's always 0, regardless of the
-- number of segments in the cluster.)
CREATE FUNCTION corrupt_file(data_file text, corruption_offset int4)
RETURNS setof integer as $$
  import os;

  if not os.path.isfile(data_file):
    plpy.info('skipping non-existent file %s' % (data_file))
  else:
    plpy.info('corrupting file %s at %s' % (data_file, corruption_offset))

    with open(data_file , "rb+") as f:
      char_location=0
      write_char='*'.encode()  # CONST.CORRUPTION

      if corruption_offset >= 0:
        f.seek(corruption_offset, 0)
      else:
        f.seek(corruption_offset, 2)

      f.write(write_char)
      f.close()

  return []
$$ LANGUAGE plpython3u EXECUTE ON ALL SEGMENTS;

CREATE OR REPLACE FUNCTION invalidate_buffers_for_rel(tablename text) RETURNS BOOL AS
$$
DECLARE
tablespace Oid;
database Oid;
relfile Oid;
result bool;
BEGIN
    SELECT dattablespace, oid INTO tablespace, database FROM pg_database WHERE datname = current_database();
    SELECT relfilenode INTO relfile FROM pg_class WHERE relname = tablename;
    SELECT public.invalidate_buffers(tablespace, database, relfile) INTO result;
    RETURN result;
END;
$$ LANGUAGE plpgsql;

-- Make sure that checksum is enabled
SHOW data_checksums;

-- skip FTS probes always
SELECT gp_inject_fault_infinite('fts_probe', 'skip', 1);
SELECT gp_request_fts_probe_scan();
select gp_wait_until_triggered_fault('fts_probe', 1, 1);

--  Corrupt a heap table
create table corrupt_table(a int);
insert into corrupt_table select i from generate_series(1, 10) i;
checkpoint;
select invalidate_buffers_for_rel('corrupt_table') from gp_dist_random('gp_id');
-- Verify corruption on heap table
select corrupt_file(get_relation_path('corrupt_table'), -50);
SELECT COUNT(*) FROM corrupt_table;

-- Corrupt a heap table, with toast table 
create table corrupt_toast_table(a int, comment bytea);
insert into corrupt_toast_table select i, ("decode"(repeat('a',3000000),'escape')) from generate_series(1,10) i;
checkpoint;
select invalidate_buffers_for_rel(get_toast_name('corrupt_toast_table')) from gp_dist_random('gp_id');
-- Verify corruption on toast table
select corrupt_file(get_toast_path('corrupt_toast_table'), -50);
SELECT md5(comment::text) FROM corrupt_toast_table;

-- Corrupt a Btree Index
create table corrupt_btree_index(a int, b char(50)); 
create index btree_index on corrupt_btree_index(a);
insert into corrupt_btree_index select i, 'a' from generate_series(1, 10) i;
checkpoint;
select invalidate_buffers_for_rel('btree_index') from gp_dist_random('gp_id');
-- Verify corruption on Btree index
select corrupt_file(get_index_path('corrupt_btree_index'), -50);
insert into corrupt_btree_index select i, 'a' from generate_series(1, 10) i; -- insert will trigger scan of the index

-- Corrupt a Bitmap Index 
create table corrupt_bitmap_index(a int, b char(50)); 
create index bitmap_index on corrupt_bitmap_index(a);
insert into corrupt_bitmap_index select i, 'a' from generate_series(1, 10) i;
checkpoint;
select invalidate_buffers_for_rel('bitmap_index') from gp_dist_random('gp_id');
-- Verify corruption on Bitmap index 
select corrupt_file(get_index_path('corrupt_bitmap_index'), -50);
insert into corrupt_bitmap_index select i, 'a' from generate_series(1, 10) i; -- insert will trigger scan of the index

-- Test make sure full page image is captured in XLOG if hint bit change is the first change after checkpoint
create table mark_buffer_dirty_hint(a int) distributed by (a);
insert into mark_buffer_dirty_hint select generate_series (1, 10);
-- buffer is marked dirty upon a hint bit change
set gp_disable_tuple_hints=off;
-- flush the data to disk
checkpoint;
-- skip all further checkpoint
select gp_inject_fault_infinite('checkpoint', 'skip', dbid) from gp_segment_configuration where role = 'p';
-- set the hint bit on (the buffer will be marked dirty)
select count(*) from mark_buffer_dirty_hint;
-- using a DML to trigger the XLogFlush() to have the backup block written
create table flush_xlog_page_to_disk (c int);
-- corrupt the page on disk
select corrupt_file(get_relation_path('mark_buffer_dirty_hint'), -50);
-- invalidate buffer and confirm data is corrupted
select invalidate_buffers_for_rel('mark_buffer_dirty_hint') from gp_dist_random('gp_id');
select count(*) from mark_buffer_dirty_hint;
-- trigger recovery on primaries with multiple retries and ignore warning/notice messages
select gp_inject_fault_infinite('finish_prepared_after_record_commit_prepared', 'panic', dbid) from gp_segment_configuration where role = 'p';
set client_min_messages='ERROR';
create table trigger_recovery_on_primaries(c int);
reset client_min_messages;
-- reconnect to the database after restart
\c 
-- EXPECT: Full Page Image (FPI) in XLOG should overwrite the corrupted page during recovery
select count(*) from corrupt_heap_checksum.mark_buffer_dirty_hint;
-- reset the fault injector
select gp_inject_fault('checkpoint', 'reset', dbid) from gp_segment_configuration where role = 'p';
select gp_inject_fault('finish_prepared_after_record_commit_prepared', 'reset', dbid) from gp_segment_configuration where role = 'p';

-- Clean up. We don't want to leave the corrupt tables lying around!
reset search_path;
DROP SCHEMA corrupt_heap_checksum CASCADE;
-- resume fts
SELECT gp_inject_fault('fts_probe', 'reset', 1);
