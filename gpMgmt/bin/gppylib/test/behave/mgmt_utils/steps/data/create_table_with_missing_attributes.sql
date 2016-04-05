-- SQL file to remote catalog attributes from table foo

SET allow_system_table_mods='dml';
DELETE FROM pg_attribute where attrelid='foo'::regclass::oid;

