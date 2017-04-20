
SET search_path = session_state;

BEGIN;

DROP VIEW session_level_memory_consumption; 
DROP FUNCTION session_state_memory_entries_f();
DROP SCHEMA session_state;

COMMIT; 
