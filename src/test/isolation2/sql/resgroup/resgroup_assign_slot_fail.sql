-- If the function AssignResGroupOnCoordinator() fails after getting a slot,
-- test the slot will be unassigned correctly.

DROP ROLE IF EXISTS role_test;
-- start_ignore
DROP RESOURCE GROUP rg_test;
-- end_ignore
CREATE RESOURCE GROUP rg_test WITH (concurrency=2, cpu_max_percent=10);
CREATE ROLE role_test RESOURCE GROUP rg_test;

1: SET ROLE role_test;
1: BEGIN;
2: SET ROLE role_test;
-- start_ignore
SELECT gp_inject_fault('resgroup_assigned_on_coordinator', 'reset', 1);
SELECT gp_inject_fault('resgroup_assigned_on_coordinator', 'error', 1);
-- end_ignore
2: BEGIN;
2: BEGIN;
1: END;
2: END;
1q:
2q:

--clean up
DROP ROLE role_test;
DROP RESOURCE GROUP rg_test;
