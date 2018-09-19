-- WARNING
-- This file is executed against the postgres database, as that is known to
-- exist at the time of running upgrades. If objects are to be manipulated
-- in other databases, make sure to change to the correct database first.

-- drop all AO tables
\ir pre_drop_ao.sql

DROP DATABASE IF EXISTS isolation2test;

\c regression;

-- drop all AO tables
\ir pre_drop_ao.sql

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

-- Missing toast tables/indexes in new cluster
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_11_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_17_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_196_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_19_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_1_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_1_true_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_23_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_32_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_3_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_4_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_4_true_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_char_variable_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_11_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_17_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_196_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_19_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_1_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_1_true_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_23_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_32_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_3_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_4_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_4_true_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_double_variable_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_11_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_17_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_196_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_19_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_1_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_1_true_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_23_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_32_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_3_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_4_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_4_true_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int2_variable_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_11_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_17_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_196_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_19_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_1_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_1_true_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_23_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_32_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_3_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_4_false_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_4_true_false CASCADE;
DROP TABLE IF EXISTS adp_dropped.alter_distpol_g_int4_variable_false_false CASCADE;
DROP TABLE IF EXISTS public.distrib_part_test CASCADE;
DROP TABLE IF EXISTS public.dml_ao_s CASCADE;
DROP TABLE IF EXISTS public.dml_co_s CASCADE;
DROP TABLE IF EXISTS public.dml_heap_s CASCADE;
DROP TABLE IF EXISTS public.test_tsvector CASCADE;
DROP TABLE IF EXISTS stat_heap3.stat_heap_t3 CASCADE;
DROP TABLE IF EXISTS stat_heap3.stat_part_heap_t3 CASCADE;
DROP TABLE IF EXISTS stat_heap4.stat_heap_t4 CASCADE;
DROP TABLE IF EXISTS stat_heap4.stat_part_heap_t4 CASCADE;
DROP TABLE IF EXISTS stat_heap5.stat_heap_t5 CASCADE;
DROP TABLE IF EXISTS stat_heap5.stat_part_heap_t5 CASCADE;
DROP TABLE IF EXISTS stat_heap6.stat_heap_t6 CASCADE;
DROP TABLE IF EXISTS stat_heap6.stat_part_heap_t6 CASCADE;

-- This one's interesting:
--    No match found in new cluster for old relation with OID 173472 in database "regression": "public.sales_1_prt_bb_pkey" which is an index on "public.newpart"
--    No match found in old cluster for new relation with OID 556718 in database "regression": "public.newpart_pkey" which is an index on "public.newpart"
DROP TABLE IF EXISTS public.newpart CASCADE;

-- This view definition changes after upgrade.
DROP VIEW IF EXISTS v_xpect_triangle_de CASCADE;

-- The dump location for this protocol changes sporadically and causes a false
-- negative. This may indicate a bug in pg_dump's sort priority for PROTOCOLs.
DROP PROTOCOL IF EXISTS demoprot_untrusted;

\c dsp1;

-- drop all AO tables
\ir pre_drop_ao.sql

-- h1 seems to switch AO storage options after upgrade.
DROP TABLE IF EXISTS public.h1 CASCADE;

\c dsp2;

-- drop all AO tables
\ir pre_drop_ao.sql

\c dsp3;

-- drop all AO tables
\ir pre_drop_ao.sql

-- toast table and index aren't correctly migrated for this relation, for some
-- reason
DROP TABLE IF EXISTS public.alter_table_reorg_heap CASCADE;
