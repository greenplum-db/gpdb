-- Adjust this setting to control where the objects get created.

CREATE SCHEMA session_state;
SET search_path = session_state;

BEGIN;


-- SessionState views
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- @function:
--        gp_session_state_memory_entries_f
--
-- @in:
--
-- @out:
--        int - segment id,
--        int - session id,
--        int - vmem in MB,
--        int - the runaway status of the session,
--        int - number of QEs,
--        int - number of QEs that already freed their memory,
--        int - amount of vmem allocated at the time it was flagged as runaway
--        int - command count running at the time it was flagged as runaway
--        TimeStampTz - last time a QE of this session became idle
--
-- @doc:
--        UDF to retrieve memory usage entries for sessions
--
--------------------------------------------------------------------------------

CREATE FUNCTION session_state_memory_entries_f_on_master()
RETURNS SETOF record
AS '$libdir/gp_session_state_memory_stats', 'gp_session_state_memory_entries'
LANGUAGE C VOLATILE EXECUTE ON MASTER;

GRANT EXECUTE ON FUNCTION session_state_memory_entries_f_on_master() TO public;

CREATE FUNCTION session_state_memory_entries_f_on_segments()
RETURNS SETOF record
AS '$libdir/gp_session_state_memory_stats', 'gp_session_state_memory_entries'
LANGUAGE C VOLATILE EXECUTE ON ALL SEGMENTS;

GRANT EXECUTE ON FUNCTION session_state_memory_entries_f_on_segments() TO public;

--------------------------------------------------------------------------------
-- @view:
--        session_level_memory_consumption
--
-- @doc:
--        List of memory usage entries for sessions
--
--------------------------------------------------------------------------------

CREATE VIEW session_level_memory_consumption AS
WITH all_entries AS (
   SELECT C.*
          FROM session_state_memory_entries_f_on_master() AS C (
            segid int,
            sessionid int,
            vmem_mb int,
            runaway_status int,
            qe_count int,
            active_qe_count int,
            dirty_qe_count int,
            runaway_vmem_mb int,
            runaway_command_cnt int,
            idle_start timestamp with time zone
          )
    UNION ALL
    SELECT C.*
          FROM session_state_memory_entries_f_on_segments() AS C (
            segid int,
            sessionid int,
            vmem_mb int,
            runaway_status int,
            qe_count int,
            active_qe_count int,
            dirty_qe_count int,
            runaway_vmem_mb int,
            runaway_command_cnt int,
            idle_start timestamp with time zone
          ))
SELECT S.datname,
       M.sessionid as sess_id,
       S.usename,
       S.current_query as current_query,
       M.segid,
       M.vmem_mb,
       case when M.runaway_status = 0 then false else true end as is_runaway,
       M.qe_count,
       M.active_qe_count,
       M.dirty_qe_count,
       M.runaway_vmem_mb,
       M.runaway_command_cnt,
       idle_start
FROM all_entries M LEFT OUTER JOIN
pg_stat_activity as S
ON M.sessionid = S.sess_id;

GRANT SELECT ON session_level_memory_consumption TO public;

COMMIT;
