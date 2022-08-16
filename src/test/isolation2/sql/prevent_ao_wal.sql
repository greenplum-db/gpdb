-- For AO/AOCO tables, their WAL records are only
-- generated for replication purposes (they are not used for crash
-- recovery because AO/AOCO table operations are crash-safe). To decrease
-- disk space usage and to improve performance of AO/AOCO operations, we
-- suppress generation of XLOG_APPENDONLY_INSERT and
-- XLOG_APPENDONLY_TRUNCATE WAL records when wal_level=minimal is
-- specified.
-- This test is supposed to ensure that XLOG_APPENDONLY_INSERT and
-- XLOG_APPENDONLY_TRUNCATE WAL records are not generated when
-- wal_level=minimal is set.
-- Because on mirrored cluster primary segments have replication slots
-- and that conflict with the wal_level=minimal GUC
-- we connect to coordinator in utility mode for AO/AOCO operations and
-- validate WAL records on the coordinator.

-- start_matchignore
-- m/pg_xlogdump: FATAL:  error in WAL record at */
-- m/.*The 'DISTRIBUTED BY' clause determines the distribution of data*/
-- m/.*Table doesn't have 'DISTRIBUTED BY' clause*/
-- end_matchignore

-- start_matchsubs
-- m/tx:\s+\d+/
-- s/tx:\s+\d+/tx: ##/

-- m/lsn: \d\/[0-9a-fA-F]+, prev \d\/[0-9a-fA-F]+/
-- s/lsn: \d\/[0-9a-fA-F]+, prev \d\/[0-9a-fA-F]+/lsn: #\/########, prev #\/########/

-- m/rel \d+\/\d+\/\d+/
-- s/rel \d+\/\d+\/\d+/rel ####\/######\/######/
-- end_matchsubs

-- Create tables (AO, AOCO)
-1U: CREATE TABLE ao_foo (n int) WITH (appendonly=true);
-1U: CREATE TABLE aoco_foo (n int, m int) WITH (appendonly=true, orientation=column);

-- Switch WAL file
-1U: SELECT true FROM pg_switch_xlog();
-- Insert data (AO)
-1U: INSERT INTO ao_foo SELECT generate_series(1,10);
-- Insert data (AOCO)
-1U: INSERT INTO aoco_foo SELECT generate_series(1,10), generate_series(1,10);
-- Delete data and run vacuum (AO)
-1U: DELETE FROM ao_foo WHERE n > 5;
-1U: VACUUM ao_foo;
-- Delete data and run vacuum (AOCO)
-1U: DELETE FROM aoco_foo WHERE n > 5;
-1U: VACUUM aoco_foo;
-1Uq:

-- Validate wal records (mirrorless setting has alternative answer file for this since wal_level is already minimal)
! last_wal_file=$(psql -At -c "SELECT pg_xlogfile_name(pg_current_xlog_location())" postgres) && pg_xlogdump ${last_wal_file} -p ${MASTER_DATA_DIRECTORY}/pg_xlog -r appendonly;

-- *********** Set wal_level=minimal **************
!\retcode gpconfig -c wal_level -v minimal --masteronly --skipvalidation;
-- Set max_wal_senders to 0 because a non-zero value requires wal_level >= 'archive'
!\retcode gpconfig -c max_wal_senders -v 0 --masteronly --skipvalidation;
-- Restart QD
!\retcode pg_ctl -l /dev/null -D $MASTER_DATA_DIRECTORY restart -w -t 600 -m fast;

-- Switch WAL file
-1U: SELECT true FROM pg_switch_xlog();
-- Insert data (AO)
-1U: INSERT INTO ao_foo SELECT generate_series(1,10);
-- Insert data (AOCO)
-1U: INSERT INTO aoco_foo SELECT generate_series(1,10), generate_series(1,10);
-- Delete data and run vacuum (AO)
-1U: DELETE FROM ao_foo WHERE n > 5;
-1U: VACUUM ao_foo;
-- Delete data and run vacuum (AOCO)
-1U: DELETE FROM aoco_foo WHERE n > 5;
-1U: VACUUM aoco_foo;

-- Validate wal records
! last_wal_file=$(psql -At -c "SELECT pg_xlogfile_name(pg_current_xlog_location())" postgres) && pg_xlogdump ${last_wal_file} -p ${MASTER_DATA_DIRECTORY}/pg_xlog -r appendonly;

-1U: DROP TABLE ao_foo;
-1U: DROP TABLE aoco_foo;

-- Reset wal_level
!\retcode gpconfig -r wal_level --masteronly --skipvalidation;
-- Reset max_wal_senders
!\retcode gpconfig -r max_wal_senders --masteronly --skipvalidation;
-- Restart QD
!\retcode pg_ctl -l /dev/null -D $MASTER_DATA_DIRECTORY restart -w -t 600 -m fast;
