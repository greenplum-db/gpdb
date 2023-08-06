/* gpcontrib/gp_toolkit/gp_toolkit--1.0--1.1.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION gp_toolkit UPDATE TO '1.1'" to load this file. \quit

--
-- Upgrade gp_toolkit.gp_workfile_entries to use gp_stat_activity. We will have
-- to drop it and its dependents first, and then recreate it and its dependents.
--

ALTER EXTENSION gp_toolkit DROP VIEW gp_toolkit.gp_workfile_entries;
ALTER EXTENSION gp_toolkit DROP VIEW gp_toolkit.gp_workfile_usage_per_segment;
ALTER EXTENSION gp_toolkit DROP VIEW gp_toolkit.gp_workfile_usage_per_query;
DROP VIEW gp_toolkit.gp_workfile_entries CASCADE;

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_workfile_entries
--
-- @doc:
--        List of all the workfile sets currently present on disk
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_workfile_entries AS
WITH all_entries AS (
    SELECT C.*
        FROM gp_toolkit.__gp_workfile_entries_f_on_coordinator() AS C (
           segid int,
           prefix text,
           size bigint,
           optype text,
           slice int,
           sessionid int,
           commandid int,
           numfiles int
        )
    UNION ALL
    SELECT C.*
        FROM gp_toolkit.__gp_workfile_entries_f_on_segments() AS C (
            segid int,
            prefix text,
            size bigint,
            optype text,
            slice int,
            sessionid int,
            commandid int,
            numfiles int
        ))
SELECT S.datname,
       S.pid,
       C.sessionid as sess_id,
       C.commandid as command_cnt,
       S.usename,
       S.query,
       C.segid,
       C.slice,
       C.optype,
       C.size,
       C.numfiles,
       C.prefix
FROM all_entries C LEFT OUTER JOIN gp_stat_activity S
ON C.sessionid = S.sess_id and C.segid=S.gp_segment_id;

GRANT SELECT ON gp_toolkit.gp_workfile_entries TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_workfile_usage_per_segment
--
-- @doc:
--        Amount of disk space used for workfiles at each segment
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_workfile_usage_per_segment AS
SELECT gpseg.content AS segid, COALESCE(SUM(wfe.size),0) AS size,
       SUM(wfe.numfiles) AS numfiles
FROM (
         SELECT content
         FROM gp_segment_configuration
         WHERE role = 'p') gpseg
         LEFT JOIN gp_toolkit.gp_workfile_entries wfe
                   ON (gpseg.content = wfe.segid)
GROUP BY gpseg.content;

GRANT SELECT ON gp_toolkit.gp_workfile_usage_per_segment TO public;

--------------------------------------------------------------------------------
-- @view:
--        gp_toolkit.gp_workfile_usage_per_query
--
-- @doc:
--        Amount of disk space used for workfiles by each query
--
--------------------------------------------------------------------------------

CREATE VIEW gp_toolkit.gp_workfile_usage_per_query AS
SELECT datname, pid, sess_id, command_cnt, usename, query, segid,
       SUM(size) AS size, SUM(numfiles) AS numfiles
FROM gp_toolkit.gp_workfile_entries
GROUP BY datname, pid, sess_id, command_cnt, usename, query, segid;

GRANT SELECT ON gp_toolkit.gp_workfile_usage_per_query TO public;
