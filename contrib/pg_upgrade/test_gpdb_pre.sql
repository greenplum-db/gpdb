-- WARNING
-- This file is executed against the postgres database, as that is known to
-- exist at the time of running upgrades. If objects are to be manipulated
-- in other databases, make sure to change to the correct database first.

DROP DATABASE IF EXISTS isolation2test;

\c regression;

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

-- We currently have a bug where a heap partition that is exchanged out of the
-- partition hierarchy is not given an array type. When this table is restored
-- in the new database, it will be given an array type -- but there will be no
-- preassigned OID for it, so it'll steal one from the pool. If some other
-- object collides with that stolen OID, upgrade will fail.
CREATE OR REPLACE FUNCTION drop_heaps_without_array_types() RETURNS void AS $$
DECLARE
	broken_table RECORD;
	sql TEXT;
BEGIN
	FOR broken_table IN
		SELECT n.nspname, c.relname
		  FROM pg_class c
		  JOIN pg_namespace n ON (n.oid = c.relnamespace)
		  JOIN pg_type t ON (t.typrelid = c.oid)
		  LEFT JOIN pg_type arr ON (arr.typelem = t.oid)
		  LEFT JOIN pg_partition_rule pr ON (pr.parchildrelid = c.oid)
		WHERE
		  -- heap relations only
		  c.relstorage = 'h' AND c.relkind = 'r'
		  -- that aren't subpartitions
		  AND pr.oid IS NULL
		  -- and that aren't catalog tables
		  AND n.nspname NOT IN ('pg_catalog', 'pg_toast', 'pg_aoseg',
							    'pg_bitmapindex', 'gp_toolkit', 'information_schema')
		  -- with no array type
		  AND arr.oid IS NULL
	LOOP
		sql := 'DROP TABLE ' || quote_ident(broken_table.nspname) || '.' || quote_ident(broken_table.relname) || ' CASCADE';

		RAISE NOTICE '%', sql;
		EXECUTE sql;
	END LOOP;
	RETURN;
END;
$$ LANGUAGE plpgsql;

SELECT drop_heaps_without_array_types();
DROP FUNCTION drop_heaps_without_array_types();

-- This one's interesting:
--    No match found in new cluster for old relation with OID 173472 in database "regression": "public.sales_1_prt_bb_pkey" which is an index on "public.newpart"
--    No match found in old cluster for new relation with OID 556718 in database "regression": "public.newpart_pkey" which is an index on "public.newpart"
DROP TABLE IF EXISTS public.newpart CASCADE;

-- This view definition changes after upgrade.
DROP VIEW IF EXISTS v_xpect_triangle_de CASCADE;

-- The dump location for this protocol changes sporadically and causes a false
-- negative. This may indicate a bug in pg_dump's sort priority for PROTOCOLs.
DROP PROTOCOL IF EXISTS demoprot_untrusted;
