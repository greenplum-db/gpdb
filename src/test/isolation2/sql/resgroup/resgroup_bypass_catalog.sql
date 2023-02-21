CREATE RESOURCE GROUP rg_test_catalog WITH (CONCURRENCY=2, CPU_HARD_QUOTA_LIMIT=10);
CREATE ROLE role_test_catalog RESOURCE GROUP rg_test_catalog;

CREATE FUNCTION rg_test_udf()
RETURNS integer AS
$$
return 1
$$
LANGUAGE plpython3u;

-- take 1 slot
1: SET ROLE role_test_catalog;
1: BEGIN;

-- take another slot
2: SET ROLE role_test_catalog;
2: BEGIN;

-- two slot have all been taken, so this query will be hung up.
3: SET ROLE role_test_catalog;
3&: BEGIN;

-- It's a catalog only query, so it will be bypassed.
4: SET ROLE role_test_catalog;
4: SELECT 1 FROM pg_catalog.pg_rules;

-- It's a udf only query, will be hung up.
-- Because there is no RangeVar, it doesn't belong to catalog only query.
5: SET ROLE role_test_catalog;
5&: SELECT rg_test_udf();

-- turn of bypass catalog query
6: SET ROLE role_test_catalog;
6: SET gp_resource_group_bypass_catalog_query = false;
6&: SELECT 1 FROM pg_catalog.pg_rules;

1: COMMIT;
2: COMMIT;
3<:
3: COMMIT;
5<:
5: COMMIT;
6<:

-- cleanup
-- start_ignore
DROP ROLE role_test_catalog;
DROP RESOURCE GROUP rg_test_catalog;
DROP FUNCTION rg_test_udf;
-- end_ignore
