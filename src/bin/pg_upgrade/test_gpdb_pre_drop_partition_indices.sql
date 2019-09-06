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
               JOIN (SELECT n.nspname AS schemaname,
                            c.relname AS tablename,
                            i.relname AS indexname
                     FROM pg_index x
                            JOIN pg_class c ON c.oid = x.indrelid
                            JOIN pg_class i ON i.oid = x.indexrelid
                            LEFT JOIN pg_inherits inh ON inh.inhrelid = x.indexrelid
                            LEFT JOIN pg_namespace n ON n.oid = c.relnamespace
                            LEFT JOIN pg_tablespace t ON t.oid = i.reltablespace
                     WHERE (c.relkind = ANY (ARRAY ['r'::"char", 'm'::"char"]))
                       AND i.relkind = 'i'::"char"
                       AND inh.inhrelid is Null) inx
                    ON (relname = inx.tablename AND nspname = inx.schemaname)
       LOOP
               EXECUTE 'DROP INDEX IF EXISTS ' || quote_ident(part_indexes.nspname) || '.' || quote_ident(part_indexes.indexname);
       END LOOP;
       RETURN;
END;
$$ LANGUAGE plpgsql;

SELECT drop_indexes();
DROP FUNCTION drop_indexes();

