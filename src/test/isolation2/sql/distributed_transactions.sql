-- Test error after ProcArrayEndTransaction

-- abort fail on QD
SELECT gp_inject_fault( 'abort_after_procarray_end', 'error', 1);
BEGIN;
CREATE TABLE test_xact_abort_failure(a int);
ABORT;
SELECT gp_inject_fault( 'abort_after_procarray_end', 'reset', 1);

-- abort fail on QE
SELECT gp_inject_fault( 'abort_after_procarray_end', 'error', dbid) from gp_segment_configuration where role = 'p' and content = 0;
BEGIN;
CREATE TABLE test_xact_abort_failure(a int);
ABORT;
SELECT gp_inject_fault( 'abort_after_procarray_end', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 0;

-- abort fail in local transaction
SELECT gp_inject_fault( 'abort_after_procarray_end', 'error', dbid) from gp_segment_configuration where role = 'p' and content = 0;
0U: BEGIN;
0U: CREATE TABLE test_xact_abort_failure(a int);
0U: ABORT;
SELECT gp_inject_fault( 'abort_after_procarray_end', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 0;
