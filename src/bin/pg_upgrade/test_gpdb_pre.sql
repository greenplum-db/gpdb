-- WARNING
-- This file is executed against the postgres database, as that is known to
-- exist at the time of running upgrades. If objects are to be manipulated
-- in other databases, make sure to change to the correct database first.

-- The isolation tests are run in utility mode, and the objects left behind
-- are not consistent between the QD and QEs.
DROP DATABASE IF EXISTS isolation_regression;

-- The rest of the things we're fixing are in the 'regression' database
\c regression

-- This one's interesting:
--    No match found in new cluster for old relation with OID 173472 in database "regression": "public.sales_1_prt_bb_pkey" which is an index on "public.newpart"
--    No match found in old cluster for new relation with OID 556718 in database "regression": "public.newpart_pkey" which is an index on "public.newpart"
-- Note: Table newpart is 'not' a partition table, and the index's referenced above are created by a check constraint.
DROP TABLE IF EXISTS public.newpart CASCADE;

-- GPDB_94_STABLE_MERGE_FIXME: The patent table has NO INHERIT
-- constraint for this table. This is not handled for pg_upgrade and
-- check if needs to be fixed or not. This table was newly added to
-- alter_table.sql as part of 9.4 STABLE merge.
DROP TABLE IF EXISTS public.test_inh_check_child CASCADE;

-- This view definition changes after upgrade.
DROP VIEW IF EXISTS v_xpect_triangle_de CASCADE;

-- Similarly, one of the partitions in this table has a DEFAULT that has
-- a cosmetic difference when it's dumped and restored twice:
--
-- CREATE TABLE public.constraint_pt1_1_prt_feb08 (
--      id integer,
--      date date,
-- -    amt numeric(10,2) DEFAULT NULL,
-- +    amt numeric(10,2) DEFAULT NULL::numeric,
--      CONSTRAINT amt_check CHECK ((amt > (0)::numeric))
--  )
--   DISTRIBUTED BY (id);
--
DROP TABLE IF EXISTS constraint_pt1;

-- The dump locations for these protocols change sporadically and cause a false
-- negative. This may indicate a bug in pg_dump's sort priority for PROTOCOLs.
DROP PROTOCOL IF EXISTS demoprot_untrusted;
DROP PROTOCOL IF EXISTS demoprot_untrusted2;

-- This is a recursive view which can only be restored in case the recursive
-- CTE guc has been turned on. Until we ship with recursive CTEs on by default
-- we need to drop this view.
DROP VIEW IF EXISTS nums CASCADE;
DROP VIEW IF EXISTS sums_1_100 CASCADE;
