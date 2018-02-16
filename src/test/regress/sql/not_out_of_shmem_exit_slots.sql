-- Used to be a bug where we release the gangs and reset context. The
-- subsequent retry succeeds with the new gang. When resetting the
-- session, as the warning message says, we drop ongoing temporary
-- namespace. However, whenever new temporary namespace is created, we
-- install shmem_exit callback for this namespace clean up. We earlier
-- missed to uninstall this callback on resetting the gang. That was
-- the reason this test exposed out of shmem_exit slots. Currently
-- MAX_ON_EXITS is set to 20 hence creates 20 transactions. Erroring
-- commit_prepared first time is used as vehicle to trigger gang
-- reset.

-- start_matchsubs
-- m/WARNING:.*Any temporary tables for this session have been dropped because the gang was disconnected/
-- s/session id \=\s*\d+/session id \= DUMMY/gm
-- end_matchsubs

CREATE TABLE foo(a int, b int);
-- 1
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 2
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 3
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 4
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 5
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 6
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 7
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 8
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 9
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 10
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 11
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 12
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 13
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 14
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 15
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 16
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 17
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 18
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 19
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;

-- 20
CREATE TEMP TABLE foo_stg AS SELECT * FROM foo;
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=commit_prepared;
SET debug_dtm_action=fail_begin_command;
DROP TABLE foo_stg;
