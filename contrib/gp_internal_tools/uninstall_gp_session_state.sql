
SET search_path = session_state;

BEGIN;

DROP VIEW session_level_memory_consumption;
DROP FUNCTION session_state_memory_entries_f_on_master();
DROP FUNCTION session_state_memory_entries_f_on_segments();
DROP SCHEMA session_state;

COMMIT;
