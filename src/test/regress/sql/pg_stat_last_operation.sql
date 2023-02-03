CREATE TABLE PG_STAT_LAST_OPERATION_TEST (foo int) DISTRIBUTED BY (foo);

SELECT classname, objname, schemaname, actionname, subtype 
FROM pg_stat_operations WHERE schemaname = 'public' 
AND objname = 'pg_stat_last_operation_test'
AND actionname = 'CREATE';

SELECT classname, objname, schemaname, actionname, subtype 
FROM pg_stat_operations WHERE schemaname = 'public' 
AND objname = 'pg_stat_last_operation_test'
AND actionname = 'TRUNCATE';

CREATE VIEW
	pg_stat_last_operation_testview_schema AS
SELECT lo.staactionname,
       lo.stasubtype,
       ns.nspname
FROM   pg_stat_last_operation lo
       join pg_class c
         ON lo.classid = c.oid
       join pg_namespace ns
         ON c.relname = 'pg_namespace'
            AND lo.objid = ns.oid;

insert into PG_STAT_LAST_OPERATION_TEST select generate_series(1,100);

truncate PG_STAT_LAST_OPERATION_TEST;

SELECT classname, objname, schemaname, actionname, subtype 
FROM pg_stat_operations WHERE schemaname = 'public' 
AND objname = 'pg_stat_last_operation_test'
AND actionname = 'TRUNCATE';

-- CREATE/DROP SCHEMA
CREATE SCHEMA mdt_schema;
SELECT * FROM pg_stat_last_operation_testview_schema WHERE nspname LIKE 'mdt_%';
DROP SCHEMA mdt_schema;
SELECT * FROM pg_stat_last_operation_testview_schema WHERE nspname LIKE 'mdt_%';
