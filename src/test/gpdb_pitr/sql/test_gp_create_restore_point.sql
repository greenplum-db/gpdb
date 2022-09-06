-- Test that gp_create_restore_point() creates a restore point WAL record
-- on the coordinator and active primary segments.

-- start_matchignore
--
-- # ignore NOTICE outputs from the test plpython function
-- m/NOTICE\:.*pg_waldump.*/
--
-- end_matchignore

-- Create plpython function that will run pg_waldump on given WAL segment
-- file and check that the given restore point record exists or not.
CREATE EXTENSION IF NOT EXISTS plpython3u;
CREATE OR REPLACE FUNCTION check_restore_point_record_generated(current_walfile_path text, restore_point_name text)
RETURNS bool AS $$
    import os

    cmd = 'pg_waldump -r xlog %s 2> /dev/null | grep %s' % (current_walfile_path, restore_point_name)
    plpy.notice('Running: %s' % cmd) # useful debug info that is match ignored
    rc = os.system(cmd)

    return (rc == 0)
$$ LANGUAGE plpython3u VOLATILE;

-- Verify that there is currently no restore point record named
-- test_gp_create_restore_point in the coordinator's and active primary
-- segments' current WAL segment file. Running pg_walfile_name here() is
-- okay due to the requirement of a fresh gpdemo cluster so timeline ids
-- should all be 1.
SELECT w.gp_segment_id,
       check_restore_point_record_generated(c.datadir || '/pg_wal/' || w.pg_walfile_name, 'test_gp_create_restore_point') AS record_found
FROM gp_segment_configuration c,
     (SELECT -1 AS gp_segment_id, pg_walfile_name(pg_current_wal_lsn())
     UNION ALL
     SELECT gp_segment_id, pg_walfile_name(pg_current_wal_lsn())
     FROM gp_dist_random('gp_id')) w
WHERE c.content = w.gp_segment_id AND c.role = 'p'
ORDER BY w.gp_segment_id;

-- Create the restore point records
SELECT true FROM gp_create_restore_point('test_gp_create_restore_point');

-- Verify that the restore point records have been created on the
-- coordinator and active primary segments.
SELECT w.gp_segment_id,
       check_restore_point_record_generated(c.datadir || '/pg_wal/' || w.pg_walfile_name, 'test_gp_create_restore_point') AS record_found
FROM gp_segment_configuration c,
     (SELECT -1 AS gp_segment_id, pg_walfile_name(pg_current_wal_lsn())
     UNION ALL
     SELECT gp_segment_id, pg_walfile_name(pg_current_wal_lsn())
     FROM gp_dist_random('gp_id')) w
WHERE c.content = w.gp_segment_id AND c.role = 'p'
ORDER BY w.gp_segment_id;

-- test simple gp_create_restore_point() error scenarios
SELECT gp_create_restore_point('this_should_fail') FROM gp_dist_random('gp_id');

CREATE TABLE this_ctas_should_fail AS SELECT gp_segment_id AS contentid, restore_lsn FROM gp_create_restore_point('this_should_fail');

CREATE ROLE create_rp_error_role;
SET ROLE TO create_rp_error_role;
SELECT * FROM gp_create_restore_point('this_should_fail_too');
RESET ROLE;
DROP ROLE create_rp_error_role;
