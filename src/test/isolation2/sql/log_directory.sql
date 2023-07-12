-- This is to test GUC log_directory
-- Expected behavior:
-- If the log_directory is outside the datadir, create the path and a child
-- directory with dbid as the name.
-- If the log_directory is within the datadir, just create it and use

-- SETUP
-- start_ignore
! gpconfig -c log_directory -v relative_log;
! gpstop -u;
-- end_ignore
! gpconfig -s log_directory;

-- SCENARIO: pg_basebackup with force-overwrite should not overwrite the
-- `log_directory` inside the data directory
! mkdir -p /tmp/datafoo/relative_log;
! echo "fakeinfo" > /tmp/datafoo/relative_log/fakefile;
SELECT pg_basebackup(address, 100, port, false, NULL, '/tmp/datafoo', true, 'stream') FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
-- verify: fakefile should still exist
! diff -r $COORDINATOR_DATA_DIRECTORY /tmp/datafoo | grep fakefile;
! rm -rf /tmp/datafoo;

-- SCENARIO: Verify absolute path works
! rm -rf /tmp/foobar;
-- start_ignore
! gpconfig -c log_directory -v /tmp/foobar;
! gpstop -u;
-- end_ignore
! gpconfig -s log_directory;
! ls /tmp/foobar;

-- verify reading the logs works for absolute paths
CREATE TABLE logdir_test_absolute(a int);
-- this should return 1
SELECT COUNT(*) FROM gp_toolkit.__gp_log_coordinator_ext WHERE logmessage LIKE 'statement: CREATE TABLE logdir_test_absolute%';

-- SCENARIO: Verify relative path works
-- start_ignore
! gpconfig -c log_directory -v still_relative;
! gpstop -u;
-- end_ignore
! gpconfig -s log_directory;

-- verify reading the logs works for non-default relative path
CREATE TABLE logdir_test_relative_path(a int);
-- this should return 1
SELECT COUNT(*) FROM gp_toolkit.__gp_log_coordinator_ext WHERE logmessage LIKE 'statement: CREATE TABLE logdir_test_relative_path%';

-- return to default
-- start_ignore
! gpconfig -r log_directory;
! gpstop -u;
-- end_ignore
! gpconfig -s log_directory;

-- cleanup
! rm -rf /tmp/foobar;
! rm -rf $COORDINATOR_DATA_DIRECTORY/still_relative;
DROP TABLE logdir_test_absolute;
DROP TABLE logdir_test_relative_path;
