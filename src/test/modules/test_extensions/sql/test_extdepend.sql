--
-- test ALTER THING name DEPENDS ON EXTENSION
--

-- Common setup for all tests
-- GPDB: We have added a serial number column (i) since the order of execution
-- of these commands matter due to inter-command dependencies.
CREATE TABLE test_extdep_commands (i int, command text);
COPY test_extdep_commands FROM stdin WITH DELIMITER ',';
 1,CREATE SCHEMA test_ext
 2,CREATE EXTENSION test_ext5 SCHEMA test_ext
 3,SET search_path TO test_ext
 4,CREATE TABLE a (a1 int)
 5,
 6,CREATE FUNCTION b() RETURNS TRIGGER LANGUAGE plpgsql AS\n   $$ BEGIN NEW.a1 := NEW.a1 + 42; RETURN NEW; END; $$
 7,ALTER FUNCTION b() DEPENDS ON EXTENSION test_ext5
 8,
 9,CREATE TRIGGER c BEFORE INSERT ON a FOR EACH ROW EXECUTE PROCEDURE b()
 10,ALTER TRIGGER c ON a DEPENDS ON EXTENSION test_ext5
 11,
 12,CREATE MATERIALIZED VIEW d AS SELECT * FROM a
 13,ALTER MATERIALIZED VIEW d DEPENDS ON EXTENSION test_ext5
 14,
 15,CREATE INDEX e ON a (a1)
 16,ALTER INDEX e DEPENDS ON EXTENSION test_ext5
 17,RESET search_path
\.
SELECT command FROM test_extdep_commands ORDER BY i;
-- First, test that dependent objects go away when the extension is dropped.
SELECT * FROM test_extdep_commands ORDER BY i \gexec
-- A dependent object made dependent again has no effect
ALTER FUNCTION test_ext.b() DEPENDS ON EXTENSION test_ext5;
-- make sure we have the right dependencies on the extension
SELECT deptype, p.*
  FROM pg_depend, pg_identify_object(classid, objid, objsubid) AS p
 WHERE refclassid = 'pg_extension'::regclass AND
       refobjid = (SELECT oid FROM pg_extension WHERE extname = 'test_ext5')
ORDER BY type;
DROP EXTENSION test_ext5;
-- anything still depending on the table?
SELECT deptype, i.*
  FROM pg_catalog.pg_depend, pg_identify_object(classid, objid, objsubid) i
WHERE refclassid='pg_class'::regclass AND
 refobjid='test_ext.a'::regclass AND NOT deptype IN ('i', 'a');
DROP SCHEMA test_ext CASCADE;

-- Second test: If we drop the table, the objects are dropped too and no
-- vestige remains in pg_depend.
SELECT command FROM test_extdep_commands ORDER BY i \gexec
DROP TABLE test_ext.a;		-- should fail, require cascade
DROP TABLE test_ext.a CASCADE;
-- anything still depending on the extension?  Should be only function b()
SELECT deptype, i.*
  FROM pg_catalog.pg_depend, pg_identify_object(classid, objid, objsubid) i
 WHERE refclassid='pg_extension'::regclass AND
 refobjid=(SELECT oid FROM pg_extension WHERE extname='test_ext5');
DROP EXTENSION test_ext5;
DROP SCHEMA test_ext CASCADE;

-- Third test: we can drop the objects individually
SELECT command FROM test_extdep_commands ORDER BY i \gexec
SET search_path TO test_ext;
DROP TRIGGER c ON a;
DROP FUNCTION b();
DROP MATERIALIZED VIEW d;
DROP INDEX e;

SELECT deptype, i.*
  FROM pg_catalog.pg_depend, pg_identify_object(classid, objid, objsubid) i
 WHERE (refclassid='pg_extension'::regclass AND
        refobjid=(SELECT oid FROM pg_extension WHERE extname='test_ext5'))
	OR (refclassid='pg_class'::regclass AND refobjid='test_ext.a'::regclass)
   AND NOT deptype IN ('i', 'a');

DROP TABLE a;
DROP SCHEMA test_ext CASCADE;
