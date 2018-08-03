-- Check if pg_stat_last_shoperation stores CREATE of global objects
CREATE DATABASE shoperation;
CREATE ROLE shoperation_user;

SELECT count(*) FROM pg_stat_last_shoperation
WHERE staactionname = 'CREATE' AND stasubtype = 'DATABASE'
AND objid = (SELECT oid FROM pg_database WHERE datname = 'shoperation');

SELECT count(*) FROM pg_stat_last_shoperation
WHERE staactionname = 'CREATE' AND stasubtype = 'ROLE'
AND objid = (SELECT oid FROM pg_authid WHERE rolname = 'shoperation_user');

-- Check if pg_stat_last_shoperation stores ALTER SET of global objects
ALTER DATABASE shoperation SET enable_seqscan TO on;
ALTER ROLE shoperation_user SET enable_seqscan TO on;

SELECT count(*) FROM pg_stat_last_shoperation
WHERE staactionname = 'ALTER' AND stasubtype = 'SET'
AND objid = (SELECT oid FROM pg_database WHERE datname = 'shoperation');

SELECT count(*) FROM pg_stat_last_shoperation
WHERE staactionname = 'ALTER' AND stasubtype = 'SET'
AND objid = (SELECT oid FROM pg_authid WHERE rolname = 'shoperation_user');

-- Check if ALTER ROLE IN DATABASE updates role object in pg_stat_last_shoperation
SET allow_system_table_mods=true;
DELETE FROM pg_stat_last_shoperation WHERE staactionname = 'ALTER' AND stasubtype = 'SET' AND objid = (SELECT oid FROM pg_authid WHERE rolname = 'shoperation_user');
RESET allow_system_table_mods;

SELECT count(*) FROM pg_stat_last_shoperation
WHERE staactionname = 'ALTER' AND stasubtype = 'SET'
AND objid = (SELECT oid FROM pg_authid WHERE rolname = 'shoperation_user');

ALTER ROLE shoperation_user IN DATABASE shoperation SET enable_seqscan TO off;

SELECT count(*) FROM pg_stat_last_shoperation
WHERE staactionname = 'ALTER' AND stasubtype = 'SET'
AND objid = (SELECT oid FROM pg_authid WHERE rolname = 'shoperation_user');

-- Clean up since we don't want these lingering
DROP ROLE shoperation_user;
DROP DATABASE shoperation;
