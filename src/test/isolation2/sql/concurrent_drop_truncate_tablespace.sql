-- While a tablespace is being dropped, if any table is created
-- in the same tablespace, the data of that table should not be deleted

-- create a tablespace directory
!\retcode mkdir -p /tmp/concurrent_tblspace;

CREATE TABLESPACE concurrent_tblspace LOCATION '/tmp/concurrent_tblspace';

-- suspend execution after TablespaceCreateLock is released
SELECT gp_inject_fault('AfterTablespaceCreateLockRelease', 'suspend', dbid) FROM gp_segment_configuration WHERE content <> -1 and role='p';
1&:DROP TABLESPACE concurrent_tblspace;

-- wait for the fault to be triggered
SELECT gp_wait_until_triggered_fault('AfterTablespaceCreateLockRelease', 1, dbid)
   from gp_segment_configuration where content <> -1 and role='p';

-- create a table in the same tablespace which is being dropped via a concurrent session
CREATE TABLE drop_tablespace_tbl(a int, b int) TABLESPACE concurrent_tblspace DISTRIBUTED BY (a);
INSERT INTO drop_tablespace_tbl SELECT i, i FROM generate_series(1,100)i;
-- reset the fault, drop tablespace command will not delete the data files on the tablespace
SELECT gp_inject_fault('AfterTablespaceCreateLockRelease', 'reset', dbid) FROM gp_segment_configuration WHERE content <> -1 and role='p';
1<:
-- check data exists
SELECT count(*) FROM drop_tablespace_tbl;
-- move to another tablespace and check the data.
ALTER TABLE drop_tablespace_tbl SET TABLESPACE pg_default;
SELECT count(*) FROM drop_tablespace_tbl;
