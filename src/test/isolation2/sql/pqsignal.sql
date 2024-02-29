-- This test ensures that we don't invoke a parent's signal handlers in any
-- child forked by the parent, before exec is performed (since after exec signal
-- handlers are discarded). We use COPY .. PROGRAM.

CREATE TABLE pgsignal_child_safety(i int);

-- Set up to suspend execution in the child after the child is forked, but
-- before we exec in the child.
SELECT gp_inject_fault('popen_with_stderr_in_child', 'suspend', dbid) FROM
    gp_segment_configuration WHERE content=-1 AND role='p';
SELECT gp_inject_fault('wrapper_handler_in_child_process', 'skip', dbid) FROM
    gp_segment_configuration WHERE content=-1 AND role='p';

1&: COPY pgsignal_child_safety FROM PROGRAM 'echo 1';

-- Wait until the child is forked.
SELECT gp_wait_until_triggered_fault('popen_with_stderr_in_child', 1, dbid)
    FROM gp_segment_configuration WHERE role='p' AND content=-1;

-- Now terminate the parent, it should signal the process group of the parent
-- and the child COPY processes.
-- Note: the child process at this point is not a registered member of the
-- procarray, so effectively we will only see one process in the following query
-- i.e. the parent.
SELECT pg_terminate_backend(pid)
    FROM pg_stat_activity where query LIKE 'COPY pgsignal_child_safety%';

SELECT gp_wait_until_triggered_fault('wrapper_handler_in_child_process', 1, dbid)
    FROM gp_segment_configuration WHERE role='p' AND content=-1;

SELECT gp_inject_fault('popen_with_stderr_in_child', 'reset', dbid) FROM
    gp_segment_configuration WHERE content=-1 AND role='p';

-- The blocked COPY session should have terminated
1<:

SELECT gp_inject_fault('wrapper_handler_in_child_process', 'reset', dbid) FROM
    gp_segment_configuration WHERE content=-1 AND role='p';
