/*
 * Greenplum System Summary Views
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * src/backend/catalog/system_views_gp_summary.sql
 *

 * This file contains summary views for various Greenplum system catalog
 * views. These summary views are designed to provide aggregated or averaged
 * information for partitioned and replicated tables, considering multiple
 * segments in a Greenplum database.
 *
 * Note: this file is read in single-user -j mode, which means that the
 * command terminator is semicolon-newline-newline; whenever the backend
 * sees that, it stops and executes what it's got.  If you write a lot of
 * statements without empty lines between, they'll all get quoted to you
 * in any error message about one of them, so don't do that.  Also, you
 * cannot write a semicolon immediately followed by an empty line in a
 * string literal (including a function body!) or a multiline comment.
 */

CREATE VIEW gp_stat_progress_vacuum_summary AS
SELECT
    max(coalesce(a1.pid, 0)) as pid,
    a.datid,
    a.datname,
    a.relid,
    a.phase,
    case when d.policytype = 'r' then (sum(a.heap_blks_total)/d.numsegments)::bigint else sum(a.heap_blks_total) end heap_blks_total,
    case when d.policytype = 'r' then (sum(a.heap_blks_scanned)/d.numsegments)::bigint else sum(a.heap_blks_scanned) end heap_blks_scanned,
    case when d.policytype = 'r' then (sum(a.heap_blks_vacuumed)/d.numsegments)::bigint else sum(a.heap_blks_vacuumed) end heap_blks_vacuumed,
    case when d.policytype = 'r' then (sum(a.index_vacuum_count)/d.numsegments)::bigint else sum(a.index_vacuum_count) end index_vacuum_count,
    case when d.policytype = 'r' then (sum(a.max_dead_tuples)/d.numsegments)::bigint else sum(a.max_dead_tuples) end max_dead_tuples,
    case when d.policytype = 'r' then (sum(a.num_dead_tuples)/d.numsegments)::bigint else sum(a.num_dead_tuples) end num_dead_tuples
FROM gp_stat_progress_vacuum a
    JOIN pg_class c ON a.relid = c.oid
    LEFT JOIN gp_distribution_policy d ON c.oid = d.localoid
    LEFT JOIN gp_stat_progress_vacuum a1 ON a.pid = a1.pid AND a1.gp_segment_id = -1
WHERE a.gp_segment_id > -1
GROUP BY a.datid, a.datname, a.relid, a.phase, d.policytype, d.numsegments;

CREATE OR REPLACE VIEW gp_stat_progress_analyze_summary AS
SELECT
    max(coalesce(a1.pid, 0)) as pid,
    a.datid,
    a.datname,
    a.relid,
    a.phase,
    case when d.policytype = 'r' then (sum(a.sample_blks_total)/d.numsegments)::bigint else sum(a.sample_blks_total) end sample_blks_total,
    case when d.policytype = 'r' then (sum(a.sample_blks_scanned)/d.numsegments)::bigint else sum(a.sample_blks_scanned) end sample_blks_scanned,
    case when d.policytype = 'r' then (sum(a.ext_stats_total)/d.numsegments)::bigint else sum(a.ext_stats_total) end ext_stats_total,
    case when d.policytype = 'r' then (sum(a.ext_stats_computed)/d.numsegments)::bigint else sum(a.ext_stats_computed) end ext_stats_computed,
    case when d.policytype = 'r' then (sum(a.child_tables_total)/d.numsegments)::bigint else sum(a.child_tables_total) end child_tables_total,
    case when d.policytype = 'r' then (sum(a.child_tables_done)/d.numsegments)::bigint else sum(a.child_tables_done) end child_tables_done
FROM gp_stat_progress_analyze a
    JOIN pg_class c ON a.relid = c.oid
    LEFT JOIN gp_distribution_policy d ON c.oid = d.localoid
    LEFT JOIN gp_stat_progress_analyze a1 ON a.pid = a1.pid AND a1.gp_segment_id = -1
WHERE a.gp_segment_id > -1
GROUP BY a.datid, a.datname, a.relid, a.phase, d.policytype, d.numsegments;

CREATE OR REPLACE VIEW gp_stat_progress_cluster_summary AS
SELECT
    max(coalesce(a1.pid, 0)) as pid,
    a.datid,
    a.datname,
    a.relid,
    a.command,
    a.phase,
    a.cluster_index_relid,
    case when d.policytype = 'r' then (sum(a.heap_tuples_scanned)/d.numsegments)::bigint else sum(a.heap_tuples_scanned) end heap_tuples_scanned,
    case when d.policytype = 'r' then (sum(a.heap_tuples_written)/d.numsegments)::bigint else sum(a.heap_tuples_written) end heap_tuples_written,
    case when d.policytype = 'r' then (sum(a.heap_blks_total)/d.numsegments)::bigint else sum(a.heap_blks_total) end heap_blks_total,
    case when d.policytype = 'r' then (sum(a.heap_blks_scanned)/d.numsegments)::bigint else sum(a.heap_blks_scanned) end heap_blks_scanned,
    case when d.policytype = 'r' then (sum(a.index_rebuild_count)/d.numsegments)::bigint else sum(a.index_rebuild_count) end index_rebuild_count
FROM gp_stat_progress_cluster a
    JOIN pg_class c ON a.relid = c.oid
    LEFT JOIN gp_distribution_policy d ON c.oid = d.localoid
    LEFT JOIN gp_stat_progress_cluster a1 ON a.pid = a1.pid AND a1.gp_segment_id = -1
WHERE a.gp_segment_id > -1
GROUP BY a.datid, a.datname, a.relid, a.command, a.phase, a.cluster_index_relid, d.policytype, d.numsegments;

CREATE OR REPLACE VIEW gp_stat_progress_create_index_summary AS
SELECT
    max(coalesce(a1.pid, 0)) as pid,
    a.datid,
    a.datname,
    a.relid,
    a.index_relid,
    a.command,
    a.phase,
    case when d.policytype = 'r' then (sum(a.lockers_total)/d.numsegments)::bigint else sum(a.lockers_total) end lockers_total,
    case when d.policytype = 'r' then (sum(a.lockers_done)/d.numsegments)::bigint else sum(a.lockers_done) end lockers_done,
    max(a.current_locker_pid) as current_locker_pid,
    case when d.policytype = 'r' then (sum(a.blocks_total)/d.numsegments)::bigint else sum(a.blocks_total) end blocks_total,
    case when d.policytype = 'r' then (sum(a.blocks_done)/d.numsegments)::bigint else sum(a.blocks_done) end blocks_done,
    case when d.policytype = 'r' then (sum(a.tuples_total)/d.numsegments)::bigint else sum(a.tuples_total) end tuples_total,
    case when d.policytype = 'r' then (sum(a.tuples_done)/d.numsegments)::bigint else sum(a.tuples_done) end tuples_done,
    case when d.policytype = 'r' then (sum(a.partitions_total)/d.numsegments)::bigint else sum(a.partitions_total) end partitions_total,
    case when d.policytype = 'r' then (sum(a.partitions_done)/d.numsegments)::bigint else sum(a.partitions_done) end partitions_done
FROM gp_stat_progress_create_index a
    JOIN pg_class c ON a.relid = c.oid
    LEFT JOIN gp_distribution_policy d ON c.oid = d.localoid
    LEFT JOIN gp_stat_progress_create_index a1 ON a.pid = a1.pid AND a1.gp_segment_id = -1
WHERE a.gp_segment_id > -1
GROUP BY a.datid, a.datname, a.relid, a.index_relid, a.command, a.phase, d.policytype, d.numsegments;
