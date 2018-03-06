DROP TABLE IF EXISTS alter_ao_part_tables_column.sto_altap3 CASCADE;
DROP TABLE IF EXISTS alter_ao_part_tables_row.sto_altap3 CASCADE;
DROP TABLE IF EXISTS co_cr_sub_partzlib8192_1_2 CASCADE;
DROP TABLE IF EXISTS co_cr_sub_partzlib8192_1 CASCADE;
DROP TABLE IF EXISTS co_wt_sub_partrle_type8192_1_2 CASCADE;
DROP TABLE IF EXISTS co_wt_sub_partrle_type8192_1 CASCADE;
DROP TABLE IF EXISTS ao_wt_sub_partzlib8192_5 CASCADE;
DROP TABLE IF EXISTS ao_wt_sub_partzlib8192_5_2 CASCADE;
DROP TABLE IF EXISTS constraint_pt1 CASCADE;
DROP TABLE IF EXISTS constraint_pt2 CASCADE;
DROP TABLE IF EXISTS constraint_pt3 CASCADE;
DROP TABLE IF EXISTS contest_inherit CASCADE;

-- Greenplum pg_upgrade doesn't support indexes on partitions since they can't
-- be reliably dump/restored in all situations. Drop all such indexes before
-- attempting the upgrade.
CREATE OR REPLACE FUNCTION drop_indexes() RETURNS void AS $$
DECLARE
	part_indexes RECORD;
BEGIN
	FOR part_indexes IN
	WITH partitions AS (
	    SELECT DISTINCT n.nspname,
	           c.relname
	    FROM pg_catalog.pg_partition p
	         JOIN pg_catalog.pg_class c ON (p.parrelid = c.oid)
	         JOIN pg_catalog.pg_namespace n ON (n.oid = c.relnamespace)
	    UNION
	    SELECT n.nspname,
	           partitiontablename AS relname
	    FROM pg_catalog.pg_partitions p
	         JOIN pg_catalog.pg_class c ON (p.partitiontablename = c.relname)
	         JOIN pg_catalog.pg_namespace n ON (n.oid = c.relnamespace)
	)
	SELECT nspname,
	       relname,
	       indexname
	FROM partitions
	     JOIN pg_catalog.pg_indexes ON (relname = tablename
										AND nspname = schemaname)
	LOOP
		EXECUTE 'DROP INDEX IF EXISTS ' || quote_ident(part_indexes.nspname) || '.' || quote_ident(part_indexes.indexname);
	END LOOP;
	RETURN;
END;
$$ LANGUAGE plpgsql;

SELECT drop_indexes();
DROP FUNCTION drop_indexes();
