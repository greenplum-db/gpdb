-- The following test simulates corruption of header and data content and
-- verifies the select behavior on this corrupted table.
--
-- Mask out the expected and actual values of the checksums when comparing the
-- result. All we care about is that they don't match.
--
-- start_matchsubs
-- m/^ERROR:  (block|header) checksum does not match./
-- s/expected 0x(........) and found 0x(........)/expected 0xXXXXXXXX and found 0xXXXXXXXX/
-- end_matchsubs

-- Ignore the status messages from the helper function. They're useful for
-- debugging, but the output is different on every invocation.
--
-- start_matchignore
-- m/^INFO:  corrupting file/
-- m/^INFO:  skipping non-existent file/
-- end_matchignore

-- start_ignore
CREATE LANGUAGE plpython3u;
-- end_ignore

-- Create our test tables (and functions) in a bespoken schema that we can drop
-- at the end. We don't want to leave any corrupt files lying around!
CREATE SCHEMA corrupt_checksums;
set search_path='corrupt_checksums';

-- to ignore the CONTEXT from messages from the plpython helpers, and to ignore
-- DETAILs from the checksum errors.
\set VERBOSITY terse

-- Return path to the file holding data for the given table (relative to
-- $PGDATA).
--
-- The returned path is for segfile #1. Because that's what we will choose for
-- the first insertion into an AO table.
CREATE FUNCTION get_aoseg1_path(tbl regclass) returns text as $$
  (select 'base/' || db.oid || '/' || c.relfilenode || '.1' from pg_class c, pg_database db where c.oid = $1 AND db.datname = current_database())
$$ language sql VOLATILE;

-- Corrupt data file at given path (if it exists on this segment)
--
-- If corruption_offset is negative, it's an offset from the end of file.
-- Otherwise it's from the beginning of file.
--
-- Returns 0. (That's handy in the way this function is called, because we can
-- do a SUM() over the return values, and it's always 0, regardless of the
-- number of segemnts in the cluster.)
CREATE FUNCTION corrupt_file(data_file text, corruption_offset int4)
RETURNS setof integer as $$
  import os;

  if not os.path.isfile(data_file):
    plpy.info('skipping non-existent file %s' % (data_file))
  else:
    plpy.info('corrupting file %s at %s' % (data_file, corruption_offset))

    with open(data_file , "rb+") as f:
      char_location=0
      write_char='*'.encode() # CONST.CORRUPTION

      if corruption_offset >= 0:
        f.seek(corruption_offset, 0)
      else:
        f.seek(corruption_offset, 2)

      f.write(write_char)
      f.close()

  return []
$$ LANGUAGE plpython3u EXECUTE ON ALL SEGMENTS;

-- Corrupt a file by replacing the last occurrence of 'str' within the file
-- with 'replacement'
CREATE FUNCTION corrupt_file(data_file text, target bytea, replacement bytea)
RETURNS setof integer as $$
  import os;

  if not os.path.isfile(data_file):
    plpy.info('skipping non-existent file %s' % (data_file))
  else:
    with open(data_file , "rb+") as f:

      content = f.read();

      # replace the last occurrence of the target string.
      pos = content.rfind(target);
      if pos == -1:
        plpy.info('target string not found in file %s, skipping' % data_file)
      else:
        plpy.info('corrupting file %s by replacing %s with %s' % (data_file, str, replacement))

        f.seek(pos, 0)
        f.write(replacement)

      f.close()

  return []
$$ LANGUAGE plpython3u EXECUTE ON ALL SEGMENTS;


-- Large content, corrupt block header
create table corrupt_header_large_co(comment bytea ) with (appendonly=true, orientation=column, checksum=true) DISTRIBUTED RANDOMLY;
insert into corrupt_header_large_co select ("decode"(repeat('a',33554432),'escape')) from generate_series(1,8)  ;
select corrupt_file(get_aoseg1_path('corrupt_header_large_co'), 8);

SELECT COUNT(*) FROM corrupt_header_large_co;

-- Large content, corrupt content
create table corrupt_content_large_co(comment bytea ) with (appendonly=true, orientation=column, checksum=true) DISTRIBUTED RANDOMLY;
insert into corrupt_content_large_co select ("decode"(repeat('a',33554432),'escape')) from generate_series(1,8)  ;
select corrupt_file(get_aoseg1_path('corrupt_content_large_co'), -3);

SELECT COUNT(*) FROM corrupt_content_large_co;

-- Small content, corrupt block header
create table corrupt_header_small_co(a int) with (appendonly=true, orientation=column, checksum=true);
insert into corrupt_header_small_co values (1),(1),(1),(-1),(1),(1),(1),(2),(2),(2),(2),(2),(2),(2),(33),(3),(3),(3),(1),(8),(19),(20),(31),(32),(33),(34),(5),(5),(5),(5),(5),(6),(6),(6),(6),(6),(6),(7),(7),(7),(7),(7),(7),(7),(7), (null),(7),(7),(7),(null),(8),(8),(8),(8),(8),(8),(4),(4),(null),(4),(17),(17),(17),(null),(null),(null);
select corrupt_file(get_aoseg1_path('corrupt_header_small_co'), 8);

SELECT COUNT(*) FROM corrupt_header_small_co;

-- Small content, corrupt content
create table corrupt_content_small_co(a int) with (appendonly=true, orientation=column, checksum=true);
insert into corrupt_content_small_co values (1),(1),(1),(-1),(1),(1),(1),(2),(2),(2),(2),(2),(2),(2),(33),(3),(3),(3),(1),(8),(19),(20),(31),(32),(33),(34),(5),(5),(5),(5),(5),(6),(6),(6),(6),(6),(6),(7),(7),(7),(7),(7),(7),(7),(7), (null),(7),(7),(7),(null),(8),(8),(8),(8),(8),(8),(4),(4),(null),(4),(17),(17),(17),(null),(null),(null);
select corrupt_file(get_aoseg1_path('corrupt_content_small_co'), -3);

SELECT COUNT(*) FROM corrupt_content_small_co;

-- Row-oriented, Small content, corrupt block header
create table corrupt_header_small_ao(a int) with (appendonly=true, orientation=row, checksum=true);
insert into corrupt_header_small_ao values (1),(1),(1),(-1),(1),(1),(1),(2),(2),(2),(2),(2),(2),(2),(33),(3),(3),(3),(1),(8),(19),(20),(31),(32),(33),(34),(5),(5),(5),(5),(5),(6),(6),(6),(6),(6),(6),(7),(7),(7),(7),(7),(7),(7),(7), (null),(7),(7),(7),(null),(8),(8),(8),(8),(8),(8),(4),(4),(null),(4),(17),(17),(17),(null),(null),(null);
select corrupt_file(get_aoseg1_path('corrupt_header_small_ao'), 8);

SELECT COUNT(*) FROM corrupt_header_small_ao;

-- Row-oriented, Small content, corrupt content
create table corrupt_content_small_ao(a int) with (appendonly=true, orientation=row, checksum=true);
insert into corrupt_content_small_ao values (1),(1),(1),(-1),(1),(1),(1),(2),(2),(2),(2),(2),(2),(2),(33),(3),(3),(3),(1),(8),(19),(20),(31),(32),(33),(34),(5),(5),(5),(5),(5),(6),(6),(6),(6),(6),(6),(7),(7),(7),(7),(7),(7),(7),(7), (null),(7),(7),(7),(null),(8),(8),(8),(8),(8),(8),(4),(4),(null),(4),(17),(17),(17),(null),(null),(null);
select corrupt_file(get_aoseg1_path('corrupt_content_small_ao'), -3);

SELECT COUNT(*) FROM corrupt_content_small_ao;


-- Also test gp_appendonly_verify_block_checksums=off.
create table appendonly_verify_block_checksums_co (t text)  with (checksum=true, appendonly=true, orientation=column, compresstype=none);
insert into appendonly_verify_block_checksums_co
  select 'abcdefghijlmnopqrstuvxyz' from generate_series(1, 5);

-- Corrupt the table by flip the 'xyz' on the last row with ###
select corrupt_file(get_aoseg1_path('appendonly_verify_block_checksums_co'), 'xyz', '###');

-- Fails, checksum is wrong.
SELECT * FROM appendonly_verify_block_checksums_co;

-- try again, ignoring the checksum failure.
set gp_appendonly_verify_block_checksums = off;
SELECT * FROM appendonly_verify_block_checksums_co;


-- Clean up. We don't want to leave the corrupt tables lying around!
reset search_path;
DROP SCHEMA corrupt_checksums CASCADE;
