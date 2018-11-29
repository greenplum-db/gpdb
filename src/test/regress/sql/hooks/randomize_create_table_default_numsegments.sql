-- start_ignore

-- HOOK NAME: randomize_create_table_default_numsegments
-- HOOK TYPE: prehook
-- HOOK DESCRIPTION:
--   when used as prehook all the tables wiil be created with random
--   numsegments.

create extension if not exists gp_debug_numsegments;
select gp_debug_reset_create_table_default_numsegments('random');

-- end_ignore
