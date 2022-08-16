-- Test that ALTER TABLE ... OWNER TO recurses to child partitions when run on the root partition in utility mode for coordinator.
CREATE ROLE testrole;
CREATE TABLE p_alterowner_1 (id INTEGER, name TEXT) DISTRIBUTED BY (id) PARTITION BY RANGE(id) (START(1) END(3) EVERY(1));

-1U: ALTER TABLE p_alterowner_1 OWNER TO testrole;
-1U: SELECT c.relname, pg_catalog.pg_get_userbyid(c.relowner) AS owner FROM pg_class c WHERE relname like 'p_alterowner_1%';

DROP TABLE p_alterowner_1;
DROP ROLE testrole;
