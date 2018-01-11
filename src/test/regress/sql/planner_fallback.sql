-- The planner enable_* GUCs work differently in GPDB than in PostgreSQL.
-- In PostgreSQL, they just add a very high cost to the cost estimates
-- of disabled plan types, but in GPDB, we don't generate such plans to
-- begin with. A consequence of that is that if there is no other possible
-- plans, you get an error.
--
-- There is a fallback mechanism, however, which is enabled by default,
-- where the planner will try again, as if the enable_* GUCs were 'on',
-- if it fails to generate a plan at first.
--
-- None of this applies to ORCA, so disable it. Otherwise we'd have to
-- have an alternative expected output file for the ORCA case, where
-- it would seem like the GUC doesn't work.
--
SET optimizer=off;

CREATE TEMPORARY TABLE fallbacktest (id int4);
INSERT INTO fallbacktest VALUES (1), (2);

-- First test without the fallback mechanism
set gp_enable_fallback_plan = off;

-- The fallback GUC doesn't affect a sequential scan on a table. We
-- always fall back if an index cannot be used.
set enable_seqscan=off;
SELECT * FROM fallbacktest;

-- A nested loop join is the only way to perform a cartesian product.
-- The fallback GUC does affect that.
set enable_nestloop=off;
SELECT * FROM fallbacktest a, fallbacktest b;

-- Try again, with fallback
set gp_enable_fallback_plan = on;
SELECT * FROM fallbacktest a, fallbacktest b;
