-- Export distributed snapshot tests

-- start_matchsubs
-- m/[0-9a-fA-F]+-[0-9a-fA-F]+-\d/
-- s/[0-9a-fA-F]+-[0-9a-fA-F]+-\d/#######-########-#/

-- m/^DETAIL:  The source process with PID \d+ is not running anymore./
-- s/^DETAIL:  The source process with PID \d+ is not running anymore./DETAIL:  The source process with PID is not running anymore./
-- end_matchsubs

-- start_ignore
DROP FUNCTION IF EXISTS corrupt_snapshot_file(text, text);
DROP FUNCTION IF EXISTS snapshot_file_ds_fields_exist(text);
DROP LANGUAGE IF EXISTS plpython3u cascade;
DROP TABLE IF EXISTS export_distributed_snapshot_test1;
-- end_ignore

CREATE LANGUAGE plpython3u;

-- Corrupt field entry for given snapshot file
CREATE OR REPLACE FUNCTION  corrupt_snapshot_file(token text, field text) RETURNS integer as
$$
  import os
  content = bytearray()
  query = "select (select datadir from gp_segment_configuration where role='p' and content=-1) || '/pg_snapshots/' as path"
  rv = plpy.execute(query)
  abs_path = rv[0]['path']
  snapshot_file = abs_path + token
  if not os.path.isfile(snapshot_file):
    plpy.info('skipping non-existent file %s' % (snapshot_file))
  else:
    plpy.info('corrupting file %s for field %s' % (snapshot_file, field))
    with open(snapshot_file , "rb+") as f:
      for line in f:
        l = line.decode()
        id = l.split(":")[0]
        if field == id:
          corrupt = l[:-2] + '*' + l[len(l)-1:]
          content.extend(corrupt.encode())
        else:
          content.extend(line)
      f.seek(0)
      f.truncate
      f.write(content)
      f.close()
  return 0
$$ LANGUAGE plpython3u;

-- Determine if field exists for given snapshot file
CREATE OR REPLACE FUNCTION  snapshot_file_ds_fields_exist(token text) RETURNS boolean as
$$
  import os
  content = bytearray()
  query = "select (select datadir from gp_segment_configuration where role='p' and content=-1) || '/pg_snapshots/' as path"
  rv = plpy.execute(query)
  abs_path = rv[0]['path']
  snapshot_file = abs_path + token
  if not os.path.isfile(snapshot_file):
    plpy.info('snapshot file %s does not exist' % (snapshot_file))
    return -1
  else:
    plpy.info('checking file %s for ds fields' % (snapshot_file))
    with open(snapshot_file , "rb+") as f:
      for line in f:
        l = line.decode()
        if "ds" in l:
          return True
  return False
$$ LANGUAGE plpython3u;

-- INSERT test
CREATE TABLE export_distributed_snapshot_test1 (a int);
INSERT INTO export_distributed_snapshot_test1 values(1);

1: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
1: @post_run ' TOKEN=`echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && echo "${RAW_STR}"': SELECT pg_export_snapshot();

INSERT INTO export_distributed_snapshot_test1 values(2);
SELECT * FROM  export_distributed_snapshot_test1;

-- Transaction 2 should return 2 rows
2: SELECT * FROM  export_distributed_snapshot_test1;
2: COMMIT;

2: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
2: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': SET TRANSACTION SNAPSHOT '@TOKEN';
-- Transaction 2 should now return 1 row
2: SELECT * FROM  export_distributed_snapshot_test1;
2: COMMIT;

1: COMMIT;

-- DELETE test
CREATE TABLE export_distributed_snapshot_test2 (a int);
INSERT INTO export_distributed_snapshot_test2 SELECT a FROM generate_series(1,3) a;

1: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
1: @post_run ' TOKEN=`echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && echo "${RAW_STR}"': SELECT pg_export_snapshot();

DELETE FROM export_distributed_snapshot_test2 WHERE a=1;

-- Should return 3 rows
1: SELECT * FROM export_distributed_snapshot_test2 ;

-- Should return 2 rows
2: SELECT * FROM  export_distributed_snapshot_test2;

2: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
2: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': SET TRANSACTION SNAPSHOT '@TOKEN';

-- Should return 3 rows
2: SELECT * FROM  export_distributed_snapshot_test2;
2: COMMIT;

1: COMMIT;

-- UPDATE test
CREATE TABLE export_distributed_snapshot_test3 (a int);
INSERT INTO export_distributed_snapshot_test3 SELECT a FROM generate_series(1,5) a;

1: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
1: @post_run ' TOKEN=`echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && echo "${RAW_STR}"': SELECT pg_export_snapshot();

UPDATE export_distributed_snapshot_test3 SET a=99 WHERE a=1;

-- Should return 0 rows
1: SELECT * FROM export_distributed_snapshot_test3 WHERE a=99;

-- Should return 1 row
2: SELECT * FROM export_distributed_snapshot_test3 WHERE a=99;

2: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
2: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': SET TRANSACTION SNAPSHOT '@TOKEN';

-- Should return 0 rows
2: SELECT * FROM export_distributed_snapshot_test3 WHERE a=99;
2: COMMIT;

-- Should return 1 row
2: SELECT * FROM export_distributed_snapshot_test3 WHERE a=99;

1: COMMIT;

-- DROP test
CREATE TABLE export_distributed_snapshot_test4 (a int);

1: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
2: BEGIN;

1: @post_run ' TOKEN=`echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && echo "${RAW_STR}"': SELECT pg_export_snapshot();
1: SELECT gp_segment_id, relname from gp_dist_random('pg_class') where relname = 'export_distributed_snapshot_test4';

-- Drop table in transaction
2: DROP TABLE export_distributed_snapshot_test4;
2: COMMIT;

3: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
3: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': SET TRANSACTION SNAPSHOT '@TOKEN';
-- The table should still be visible to transaction 3 using transaction 1's snapshot.
3: SELECT gp_segment_id, relname from gp_dist_random('pg_class') where relname = 'export_distributed_snapshot_test4';
3: COMMIT;
-- The table should no longer be visible to transaction 3.
3: SELECT gp_segment_id, relname from gp_dist_random('pg_class') where relname = 'export_distributed_snapshot_test4';

1: COMMIT;

-- Test corrupt fields in snapshot file

-- xmin
1: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

1: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
1: @post_run ' TOKEN=`echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && echo "${RAW_STR}"': SELECT pg_export_snapshot();

2: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': select corrupt_snapshot_file('@TOKEN', 'xmin');

2: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
2: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': SET TRANSACTION SNAPSHOT '@TOKEN';

1: END;
2: END;

-- dsxminall

1: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

1: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
1: @post_run ' TOKEN=`echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && echo "${RAW_STR}"': SELECT pg_export_snapshot();

2: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': select corrupt_snapshot_file('@TOKEN', 'dsxminall');

2: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
2: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': SET TRANSACTION SNAPSHOT '@TOKEN';

1: END;
2: END;

-- dsxmin
1: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;

1: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
1: @post_run ' TOKEN=`echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && echo "${RAW_STR}"': SELECT pg_export_snapshot();

2: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': select corrupt_snapshot_file('@TOKEN', 'dsxmin');

2: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
2: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': SET TRANSACTION SNAPSHOT '@TOKEN';

1: END;
2: END;

-- Test export snapshot in utility mode does not export distributed snapshot fields

-1U: BEGIN;
-1U: BEGIN;
-1U: @post_run ' TOKEN=`echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && echo "${RAW_STR}"': SELECT pg_export_snapshot();

-- Should return false
1: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': select snapshot_file_ds_fields_exist('@TOKEN');

-1Uq:
1: END;

-- Test import snapshot in utility mode fails if distributed snapshot fields exist
1: BEGIN;
1: BEGIN;
1: @post_run ' TOKEN=`echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && echo "${RAW_STR}"': SELECT pg_export_snapshot();

-- Open utility mode connection on coordinator and set snapshot
-1U: BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
-1U: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': SET TRANSACTION SNAPSHOT '@TOKEN';

-1Uq:
1: END;

-- Test export snapshot in utility mode and import snapshot in utility mode succeeds
-1U: @db_name postgres: BEGIN;
-1U: BEGIN;
-1U: @post_run ' TOKEN=`echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && echo "${RAW_STR}"': SELECT pg_export_snapshot();

-- Should return false
1: @pre_run 'echo "${RAW_STR}" | sed "s#@TOKEN#${TOKEN}#"': select snapshot_file_ds_fields_exist('@TOKEN');

-- Open another utility mode connection and set the snapshot
! TOKEN=$(find ${COORDINATOR_DATA_DIRECTORY}/pg_snapshots/ -name "0*" -exec basename {} \;) \
&& echo ${TOKEN} \
&& PGOPTIONS='-c gp_role=utility' psql postgres -Atc "BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ; SET TRANSACTION SNAPSHOT '${TOKEN}';";

-1Uq:
