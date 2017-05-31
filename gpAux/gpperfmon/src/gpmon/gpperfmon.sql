--  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
--  Gpperfmon Schema

-- Note: In 4.x, this file was run as part of upgrade (in single user mode).
-- Therefore, we could not make use of psql escape sequences such as
-- "\c gpperfmon" and every statement had to be on a single line.
--
-- Violating the above _would_ break 4.x upgrades.
--

--  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
--  system
--
\c gpperfmon;

create table public.system_history (
       ctime timestamp(0) not null, -- record creation time
       hostname varchar(64) not null, -- hostname of system this metric belongs to
       mem_total bigint not null, mem_used bigint not null, -- total system memory
       mem_actual_used bigint not null, mem_actual_free bigint not null, -- memory used
       swap_total bigint not null, swap_used bigint not null, -- total swap space
       swap_page_in bigint not null, swap_page_out bigint not null, -- swap pages in
       cpu_user float not null, cpu_sys float not null, cpu_idle float not null, -- cpu usage
       load0 float not null, load1 float not null, load2 float not null, -- cpu load avgs
       quantum int not null, -- interval between metric collection for this entry
       disk_ro_rate bigint not null, -- system disk read ops per second
       disk_wo_rate bigint not null, -- system disk write ops per second
       disk_rb_rate bigint not null, -- system disk read bytes per second
       disk_wb_rate bigint not null, -- system disk write bytes per second
       net_rp_rate bigint not null,  -- system net read packets per second
       net_wp_rate bigint not null,  -- system net write packets per second
       net_rb_rate bigint not null,  -- system net read bytes per second
       net_wb_rate bigint not null   -- system net write bytes per second
) 
with (fillfactor=100)
distributed by (ctime)
partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));

create external web table public.system_now (
       like public.system_history
) execute 'cat gpperfmon/data/system_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


create external web table public.system_tail (
       like public.system_history
) execute 'cat gpperfmon/data/system_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


create external web table public._system_tail (
        like public.system_history
) execute 'cat gpperfmon/data/_system_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


--  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
--  queries
--

create table public.queries_history (
       ctime timestamp(0), -- record creation time
       tmid int not null,  -- time id
       ssid int not null,    -- session id
       ccnt int not null,    -- command count in session
       username varchar(64) not null, -- username that issued the query
       db varchar(64) not null, -- database name for the query
       cost int not null, -- query cost (not implemented)
       tsubmit timestamp(0) not null, -- query submit time
       tstart timestamp(0),  -- query start time
       tfinish timestamp(0) not null,    -- query end time
       status varchar(64) not null,   -- query status (start, end, abort)
       rows_out bigint not null, -- rows out for query
       cpu_elapsed bigint not null, -- cpu usage for query across all segments
       cpu_currpct float not null, -- current cpu percent avg for all processes executing query
       skew_cpu float not null,    -- coefficient of variance for cpu_elapsed of iterators across segments for query
       skew_rows float not null,   -- coefficient of variance for rows_in of iterators across segments for query
       query_hash bigint not null, -- (not implemented)
       query_text text not null default '', -- query text
       query_plan text not null default '', -- query plan (not implemented)
       application_name varchar(64), -- from 4.2 onwards
       rsqname varchar(64),          -- from 4.2 onwards
       rqppriority varchar(16)       -- from 4.2 onwards
)
with (fillfactor=100)
distributed by (ctime)
partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));


create external web table public.queries_now (
        like public.queries_history
) execute 'python $GPHOME/sbin/gpmon_catqrynow.py 2> /dev/null || true' on master format 'csv' (delimiter '|' NULL as 'null');

create external web table public.queries_now_fast (
       ctime timestamp(0),
       tmid int,
       ssid int,    -- gp_session_id
       ccnt int,    -- gp_command_count
       username varchar(64),
       db varchar(64),
       cost int,
       tsubmit timestamp(0), 
       tstart timestamp(0), 
       tfinish timestamp(0),
       status varchar(64),
       rows_out bigint,
       cpu_elapsed bigint,
       cpu_currpct float,
       skew_cpu float,		-- always 0
       skew_rows float
       -- excluded: query_text text
       -- excluded: query_plan text
) execute 'cat gpperfmon/data/queries_now.dat 2> /dev/null || true' on master format 'csv' (delimiter '|' NULL as 'null');

create external web table public.queries_tail (
        like public.queries_history
) execute 'cat gpperfmon/data/queries_tail.dat 2> /dev/null || true' on master format 'csv' (delimiter '|' NULL as 'null');


create external web table public._queries_tail (
        like public.queries_history
) execute 'cat gpperfmon/data/_queries_tail.dat 2> /dev/null || true' on master format 'csv' (delimiter '|' NULL as 'null');


--  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
--  query iterators
--

create table public.iterators_history (
       ctime timestamp(0), -- record creation time
       tmid int not null,  -- time id
       ssid int not null,    -- session id
       ccnt int not null,    -- command count for session
       segid int not null,   -- segment id
       pid int not null, -- pid of process executing iterator
       nid int not null, -- node id
       pnid int not null,-- parent node id
       hostname varchar(64) not null,
       ntype varchar(64) not null, -- node type
       nstatus varchar(64) not null, -- node status
       tstart timestamp(0) not null, -- time started
       tduration int not null, -- duration of execution
       pmemsize bigint not null, -- TODO: fill in these
       pmemmax bigint not null,
       memsize bigint not null, 
       memresid bigint not null,
       memshare bigint not null,
       cpu_elapsed bigint not null, -- process cpu usage total
       cpu_currpct float not null, -- process current cpu percentage
       phase varchar(64) not null, -- iterator phase
       rows_out bigint not null default 0, -- rows out
       rows_out_est bigint not null default 0, -- planner estimate of rows out
       m0_name varchar(64) not null default '', -- iterator metric name
       m0_unit varchar(64) not null default '', -- iterator metric unit (rows, bytes, etc.)
       m0_val bigint not null default 0,         -- current metric value
       m0_est bigint not null default 0,         -- estimated final value
       m1_name varchar(64),
       m1_unit varchar(64),
       m1_val bigint,
       m1_est bigint,
       m2_name varchar(64),
       m2_unit varchar(64),
       m2_val bigint,
       m2_est bigint,
       m3_name varchar(64),
       m3_unit varchar(64),
       m3_val bigint,
       m3_est bigint,
       m4_name varchar(64),
       m4_unit varchar(64),
       m4_val bigint,
       m4_est bigint,
       m5_name varchar(64),
       m5_unit varchar(64),
       m5_val bigint,
       m5_est bigint,
       m6_name varchar(64),
       m6_unit varchar(64),
       m6_val bigint,
       m6_est bigint,
       m7_name varchar(64),
       m7_unit varchar(64),
       m7_val bigint,
       m7_est bigint,
       m8_name varchar(64),
       m8_unit varchar(64),
       m8_val bigint,
       m8_est bigint,
       m9_name varchar(64),
       m9_unit varchar(64),
       m9_val bigint,
       m9_est bigint,
       m10_name varchar(64),
       m10_unit varchar(64),
       m10_val bigint,
       m10_est bigint,
       m11_name varchar(64),
       m11_unit varchar(64),
       m11_val bigint,
       m11_est bigint,
       m12_name varchar(64),
       m12_unit varchar(64),
       m12_val bigint,
       m12_est bigint,
       m13_name varchar(64),
       m13_unit varchar(64),
       m13_val bigint,
       m13_est bigint,
       m14_name varchar(64),
       m14_unit varchar(64),
       m14_val bigint,
       m14_est bigint,
       m15_name varchar(64),
       m15_unit varchar(64),
       m15_val bigint,
       m15_est bigint,
       t0_name varchar(64),
       t0_val varchar(128)
)
with (fillfactor=100)
distributed by (ctime)
partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));



create external web table public.iterators_now (
        like public.iterators_history
) execute 'cat gpperfmon/data/iterators_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


create external web table public.iterators_tail (
        like public.iterators_history
) execute 'cat gpperfmon/data/iterators_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


create external web table public._iterators_tail (
        like public.iterators_history
) execute 'cat gpperfmon/data/_iterators_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


--  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
--  database
--

create table public.database_history (
       ctime timestamp(0) not null, -- record creation time
       queries_total int not null, -- total number of queries
       queries_running int not null, -- number of running queries
       queries_queued int not null -- number of queued queries
) 
with (fillfactor=100)
distributed by (ctime)
partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));

create external web table public.database_now (
       like public.database_history
) execute 'cat gpperfmon/data/database_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


create external web table public.database_tail (
       like public.database_history
) execute 'cat gpperfmon/data/database_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


create external web table public._database_tail (
        like public.database_history
) execute 'cat gpperfmon/data/_database_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


create external web table public.master_data_dir (hostname text, dir text)
execute E'python -c "import socket, os; print socket.gethostname() + \\"|\\" + os.getcwd()"' on master
format 'csv' (delimiter '|');


--  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
--  Web API views
--

DROP AGGREGATE IF EXISTS iterators_array_accum(anyelement) CASCADE; 
CREATE AGGREGATE iterators_array_accum(anyelement) (
  SFUNC=array_append,
  STYPE=anyarray,
  INITCOND='{}'
);

DROP VIEW IF EXISTS iterators_history_rollup;
CREATE VIEW iterators_history_rollup as select 
       min(ctime) as sample_time, tmid, ssid, ccnt, nid, min(pnid) as pnid, 
       min(ntype) as ntype, array_to_string(iterators_array_accum(distinct nstatus), ',') as nstatus, 
       min(tstart) as tstart, avg(tduration) as tduration, 
       avg(pmemsize) as pmemsize, avg(pmemmax) as pmemmax, 
       avg(memsize) as memsize, avg(memresid) as memresid, 
       avg(memshare) as memshare, sum(cpu_elapsed) as cpu_elapsed, 
       avg(cpu_currpct) as cpu_currpct, array_to_string(iterators_array_accum(distinct phase), ',') as phase, 
       sum(rows_out) as rows_out, sum(rows_out_est) as rows_out_est, 
       case when avg(cpu_elapsed) <= 0.001 THEN 0 else 
          (stddev(cpu_elapsed)/avg(cpu_elapsed))*100 end as skew_cpu,
       case when avg(rows_out) <= 0.001 THEN 0 else 
          (stddev(rows_out)/avg(rows_out))*100 end as skew_rows,
       min(m0_name)||'|'||min(m0_unit)||'|'||avg(m0_val)||'|'||avg(m0_est) as m0,
       min(m1_name)||'|'||min(m1_unit)||'|'||avg(m1_val)||'|'||avg(m1_est) as m1,
       min(m2_name)||'|'||min(m2_unit)||'|'||avg(m2_val)||'|'||avg(m2_est) as m2,
       min(m3_name)||'|'||min(m3_unit)||'|'||avg(m3_val)||'|'||avg(m3_est) as m3,
       min(m4_name)||'|'||min(m4_unit)||'|'||avg(m4_val)||'|'||avg(m4_est) as m4,
       min(m5_name)||'|'||min(m5_unit)||'|'||avg(m5_val)||'|'||avg(m5_est) as m5,
       min(m6_name)||'|'||min(m6_unit)||'|'||avg(m6_val)||'|'||avg(m6_est) as m6,
       min(m7_name)||'|'||min(m7_unit)||'|'||avg(m7_val)||'|'||avg(m7_est) as m7,
       min(m8_name)||'|'||min(m8_unit)||'|'||avg(m8_val)||'|'||avg(m8_est) as m8,
       min(m9_name)||'|'||min(m9_unit)||'|'||avg(m9_val)||'|'||avg(m9_est) as m9,
       min(m10_name)||'|'||min(m10_unit)||'|'||avg(m10_val)||'|'||avg(m10_est) as m10,
       min(m11_name)||'|'||min(m11_unit)||'|'||avg(m11_val)||'|'||avg(m11_est) as m11,
       min(m12_name)||'|'||min(m12_unit)||'|'||avg(m12_val)||'|'||avg(m12_est) as m12,
       min(m13_name)||'|'||min(m13_unit)||'|'||avg(m13_val)||'|'||avg(m13_est) as m13,
       min(m14_name)||'|'||min(m14_unit)||'|'||avg(m14_val)||'|'||avg(m14_est) as m14,
       min(m15_name)||'|'||min(m15_unit)||'|'||avg(m15_val)||'|'||avg(m15_est) as m15,
       min(t0_name)||'|'||min(t0_val) as t0
       from iterators_history group by tmid, ssid, ccnt, nid;

DROP VIEW IF EXISTS iterators_tail_rollup;
CREATE VIEW iterators_tail_rollup as select 
       min(ctime) as sample_time, tmid, ssid, ccnt, nid, min(pnid) as pnid, 
       min(ntype) as ntype, array_to_string(iterators_array_accum(distinct nstatus), ',') as nstatus, 
       min(tstart) as tstart, avg(tduration) as tduration, 
       avg(pmemsize) as pmemsize, avg(pmemmax) as pmemmax, 
       avg(memsize) as memsize, avg(memresid) as memresid, 
       avg(memshare) as memshare, sum(cpu_elapsed) as cpu_elapsed, 
       avg(cpu_currpct) as cpu_currpct, array_to_string(iterators_array_accum(distinct phase), ',') as phase, 
       sum(rows_out) as rows_out, sum(rows_out_est) as rows_out_est, 
       case when avg(cpu_elapsed) <= 0.001 THEN 0 else 
          (stddev(cpu_elapsed)/avg(cpu_elapsed))*100 end as skew_cpu,
       case when avg(rows_out) <= 0.001 THEN 0 else 
          (stddev(rows_out)/avg(rows_out))*100 end as skew_rows,
       min(m0_name)||'|'||min(m0_unit)||'|'||avg(m0_val)||'|'||avg(m0_est) as m0,
       min(m1_name)||'|'||min(m1_unit)||'|'||avg(m1_val)||'|'||avg(m1_est) as m1,
       min(m2_name)||'|'||min(m2_unit)||'|'||avg(m2_val)||'|'||avg(m2_est) as m2,
       min(m3_name)||'|'||min(m3_unit)||'|'||avg(m3_val)||'|'||avg(m3_est) as m3,
       min(m4_name)||'|'||min(m4_unit)||'|'||avg(m4_val)||'|'||avg(m4_est) as m4,
       min(m5_name)||'|'||min(m5_unit)||'|'||avg(m5_val)||'|'||avg(m5_est) as m5,
       min(m6_name)||'|'||min(m6_unit)||'|'||avg(m6_val)||'|'||avg(m6_est) as m6,
       min(m7_name)||'|'||min(m7_unit)||'|'||avg(m7_val)||'|'||avg(m7_est) as m7,
       min(m8_name)||'|'||min(m8_unit)||'|'||avg(m8_val)||'|'||avg(m8_est) as m8,
       min(m9_name)||'|'||min(m9_unit)||'|'||avg(m9_val)||'|'||avg(m9_est) as m9,
       min(m10_name)||'|'||min(m10_unit)||'|'||avg(m10_val)||'|'||avg(m10_est) as m10,
       min(m11_name)||'|'||min(m11_unit)||'|'||avg(m11_val)||'|'||avg(m11_est) as m11,
       min(m12_name)||'|'||min(m12_unit)||'|'||avg(m12_val)||'|'||avg(m12_est) as m12,
       min(m13_name)||'|'||min(m13_unit)||'|'||avg(m13_val)||'|'||avg(m13_est) as m13,
       min(m14_name)||'|'||min(m14_unit)||'|'||avg(m14_val)||'|'||avg(m14_est) as m14,
       min(m15_name)||'|'||min(m15_unit)||'|'||avg(m15_val)||'|'||avg(m15_est) as m15,
       min(t0_name)||'|'||min(t0_val) as t0
       from iterators_tail group by tmid, ssid, ccnt, nid;

DROP VIEW IF EXISTS iterators_now_rollup;
CREATE VIEW iterators_now_rollup as select 
       min(ctime) as sample_time, tmid, ssid, ccnt, nid, min(pnid) as pnid, 
       min(ntype) as ntype, array_to_string(iterators_array_accum(distinct nstatus), ',') as nstatus, 
       min(tstart) as tstart, avg(tduration) as tduration, 
       avg(pmemsize) as pmemsize, avg(pmemmax) as pmemmax, 
       avg(memsize) as memsize, avg(memresid) as memresid, 
       avg(memshare) as memshare, sum(cpu_elapsed) as cpu_elapsed, 
       avg(cpu_currpct) as cpu_currpct, array_to_string(iterators_array_accum(distinct phase), ',') as phase, 
       sum(rows_out) as rows_out, sum(rows_out_est) as rows_out_est, 
       case when avg(cpu_elapsed) <= 0.001 THEN 0 else 
          (stddev(cpu_elapsed)/avg(cpu_elapsed))*100 end as skew_cpu,
       case when avg(rows_out) <= 0.001 THEN 0 else 
          (stddev(rows_out)/avg(rows_out))*100 end as skew_rows,
       min(m0_name)||'|'||min(m0_unit)||'|'||avg(m0_val)||'|'||avg(m0_est) as m0,
       min(m1_name)||'|'||min(m1_unit)||'|'||avg(m1_val)||'|'||avg(m1_est) as m1,
       min(m2_name)||'|'||min(m2_unit)||'|'||avg(m2_val)||'|'||avg(m2_est) as m2,
       min(m3_name)||'|'||min(m3_unit)||'|'||avg(m3_val)||'|'||avg(m3_est) as m3,
       min(m4_name)||'|'||min(m4_unit)||'|'||avg(m4_val)||'|'||avg(m4_est) as m4,
       min(m5_name)||'|'||min(m5_unit)||'|'||avg(m5_val)||'|'||avg(m5_est) as m5,
       min(m6_name)||'|'||min(m6_unit)||'|'||avg(m6_val)||'|'||avg(m6_est) as m6,
       min(m7_name)||'|'||min(m7_unit)||'|'||avg(m7_val)||'|'||avg(m7_est) as m7,
       min(m8_name)||'|'||min(m8_unit)||'|'||avg(m8_val)||'|'||avg(m8_est) as m8,
       min(m9_name)||'|'||min(m9_unit)||'|'||avg(m9_val)||'|'||avg(m9_est) as m9,
       min(m10_name)||'|'||min(m10_unit)||'|'||avg(m10_val)||'|'||avg(m10_est) as m10,
       min(m11_name)||'|'||min(m11_unit)||'|'||avg(m11_val)||'|'||avg(m11_est) as m11,
       min(m12_name)||'|'||min(m12_unit)||'|'||avg(m12_val)||'|'||avg(m12_est) as m12,
       min(m13_name)||'|'||min(m13_unit)||'|'||avg(m13_val)||'|'||avg(m13_est) as m13,
       min(m14_name)||'|'||min(m14_unit)||'|'||avg(m14_val)||'|'||avg(m14_est) as m14,
       min(m15_name)||'|'||min(m15_unit)||'|'||avg(m15_val)||'|'||avg(m15_est) as m15,
       min(t0_name)||'|'||min(t0_val) as t0
       from iterators_now group by tmid, ssid, ccnt, nid;

-- TABLE: segment_history
--   ctime                      record creation time
--   dbid                       segment database id
--   hostname                   hostname of system this metric belongs to
--   dynamic_memory_used        bytes of dynamic memory used by the segment
--   dynamic_memory_available   bytes of dynamic memory available for use by the segment
create table public.segment_history (ctime timestamp(0) not null, dbid int not null, hostname varchar(64) not null, dynamic_memory_used bigint not null, dynamic_memory_available bigint not null) with (fillfactor=100) distributed by (ctime) partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));

-- TABLE: segment_now
--   (like segment_history)
create external web table public.segment_now (like public.segment_history) execute 'cat gpperfmon/data/segment_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


-- TABLE: segment_tail
--   (like segment_history)
create external web table public.segment_tail (like public.segment_history) execute 'cat gpperfmon/data/segment_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: _segment_tail
--   (like segment_history)
create external web table public._segment_tail (like public.segment_history) execute 'cat gpperfmon/data/_segment_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

DROP VIEW IF EXISTS public.memory_info;
DROP VIEW IF EXISTS public.dynamic_memory_info;

-- VIEW: dynamic_memory_info
CREATE VIEW public.dynamic_memory_info as select public.segment_history.ctime, public.segment_history.hostname, round(sum(public.segment_history.dynamic_memory_used)/1024/1024, 2) AS dynamic_memory_used_mb, round(sum(public.segment_history.dynamic_memory_available)/1024/1024, 2) AS dynamic_memory_available_mb FROM public.segment_history GROUP BY public.segment_history.ctime, public.segment_history.hostname;

-- VIEW: memory_info
CREATE VIEW public.memory_info as select public.system_history.ctime, public.system_history.hostname, round(public.system_history.mem_total/1024/1024, 2) as mem_total_mb, round(public.system_history.mem_used/1024/1024, 2) as mem_used_mb, round(public.system_history.mem_actual_used/1024/1024, 2) as mem_actual_used_mb, round(public.system_history.mem_actual_free/1024/1024, 2) as mem_actual_free_mb, round(public.system_history.swap_total/1024/1024, 2) as swap_total_mb, round(public.system_history.swap_used/1024/1024, 2) as swap_used_mb, dynamic_memory_info.dynamic_memory_used_mb as dynamic_memory_used_mb, dynamic_memory_info.dynamic_memory_available_mb as dynamic_memory_available_mb FROM public.system_history, dynamic_memory_info WHERE public.system_history.hostname = dynamic_memory_info.hostname AND public.system_history.ctime = public.dynamic_memory_info.ctime;


-- TABLE: diskspace_history
--   ctime                      time of measurement
--   hostname                   hostname of measurement
--   filesytem                  name of filesystem for measurement
--   total_bytes                bytes total in filesystem
--   bytes_used                 bytes used in the filesystem
--   bytes_available            bytes available in the filesystem
create table public.diskspace_history (ctime timestamp(0) not null, hostname varchar(64) not null, filesystem text not null, total_bytes bigint not null, bytes_used bigint not null, bytes_available bigint not null) with (fillfactor=100) distributed by (ctime) partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));

--- TABLE: diskspace_now
--   (like diskspace_history)
create external web table public.diskspace_now (like public.diskspace_history) execute 'cat gpperfmon/data/diskspace_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: diskpace_tail
--   (like diskspace_history)
create external web table public.diskspace_tail (like public.diskspace_history) execute 'cat gpperfmon/data/diskspace_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: _diskspace_tail
--   (like diskspace_history)
create external web table public._diskspace_tail (like public.diskspace_history) execute 'cat gpperfmon/data/_diskspace_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


-- TABLE: network_interface_history -------------------------------------------------------------------------------------------------------------------------------------------------------------------
-- ctime timestamp(0) not null, 
-- hostname varchar(64) not null, 
-- interface_name varchar(64) not null,
-- bytes_received bigint, 
-- packets_received bigint,
-- receive_errors bigint,
-- receive_drops bigint,
-- receive_fifo_errors bigint,
-- receive_frame_errors bigint,
-- receive_compressed_packets int,
-- receive_multicast_packets int,
-- bytes_transmitted bigint,
-- packets_transmitted bigint,
-- transmit_errors bigint,
-- transmit_drops bigint,
-- transmit_fifo_errors bigint,
-- transmit_collision_errors bigint,
-- transmit_carrier_errors bigint,
-- transmit_compressed_packets int
create table public.network_interface_history ( ctime timestamp(0) not null, hostname varchar(64) not null, interface_name varchar(64) not null, bytes_received bigint, packets_received bigint, receive_errors bigint, receive_drops bigint, receive_fifo_errors bigint, receive_frame_errors bigint, receive_compressed_packets int, receive_multicast_packets int, bytes_transmitted bigint, packets_transmitted bigint, transmit_errors bigint, transmit_drops bigint, transmit_fifo_errors bigint, transmit_collision_errors bigint, transmit_carrier_errors bigint, transmit_compressed_packets int) with (fillfactor=100) distributed by (ctime) partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));

--- TABLE: network_interface_now
--   (like network_interface_history)
create external web table public.network_interface_now (like public.network_interface_history) execute 'cat gpperfmon/data/network_interface_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: network_interface_tail
--   (like network_interface_history)
create external web table public.network_interface_tail (like public.network_interface_history) execute 'cat gpperfmon/data/network_interface_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: _network_interface_tail
--   (like network_interface_history)
create external web table public._network_interface_tail (like public.network_interface_history) execute 'cat gpperfmon/data/_network_interface_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


-- TABLE: sockethistory --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
-- ctime timestamp(0) not null, 
-- hostname varchar(64) not null, 
-- total_sockets_used int,
-- tcp_sockets_inuse int,
-- tcp_sockets_orphan int,
-- tcp_sockets_timewait int,
-- tcp_sockets_alloc int,
-- tcp_sockets_memusage_inbytes int,
-- udp_sockets_inuse int,
-- udp_sockets_memusage_inbytes int,
-- raw_sockets_inuse int,
-- frag_sockets_inuse int,
-- frag_sockets_memusage_inbytes int

create table public.socket_history ( ctime timestamp(0) not null, hostname varchar(64) not null, total_sockets_used int, tcp_sockets_inuse int, tcp_sockets_orphan int, tcp_sockets_timewait int, tcp_sockets_alloc int, tcp_sockets_memusage_inbytes int, udp_sockets_inuse int, udp_sockets_memusage_inbytes int, raw_sockets_inuse int, frag_sockets_inuse int, frag_sockets_memusage_inbytes int) with (fillfactor=100) distributed by (ctime) partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month')); 

--- TABLE: socket_now
--   (like socket_history)
create external web table public.socket_now (like public.socket_history) execute 'cat gpperfmon/data/socket_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: socket_tail
--   (like socket_history)
create external web table public.socket_tail (like public.socket_history) execute 'cat gpperfmon/data/socket_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: _socket_tail
--   (like socket_history)
create external web table public._socket_tail (like public.socket_history) execute 'cat gpperfmon/data/_socket_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: gp_log_master_ext 
--   (like gp_toolkit.__gp_log_master_ext)
CREATE EXTERNAL WEB TABLE public.gp_log_master_ext (LIKE gp_toolkit.__gp_log_master_ext) EXECUTE E'find $GP_SEG_DATADIR/pg_log/ -name "gpdb*.csv" | sort -r | head -n 2 | xargs cat' ON MASTER FORMAT 'csv' (delimiter E',' null E'' escape E'"' quote E'"') ENCODING 'UTF8';

-- TABLE: log_alert_history 
--   (like gp_toolkit.__gp_log_master_ext)
CREATE TABLE public.log_alert_history (LIKE gp_toolkit.__gp_log_master_ext) distributed by (logtime) partition by range (logtime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));

-- TABLE: log_alert_tail 
--   (like gp_toolkit.__gp_log_master_ext)
CREATE EXTERNAL WEB TABLE public.log_alert_tail (LIKE public.log_alert_history) EXECUTE 'cat gpperfmon/logs/alert_log_stage 2> /dev/null || true' ON MASTER FORMAT 'csv' (delimiter E',' null E'' escape E'"' quote E'"') ENCODING 'UTF8'; 

-- TABLE: log_alert_all 
--   (like gp_toolkit.__gp_log_master_ext)
CREATE EXTERNAL WEB TABLE public.log_alert_now (LIKE public.log_alert_history) EXECUTE 'cat gpperfmon/logs/*.csv 2> /dev/null || true' ON MASTER FORMAT 'csv' (delimiter E',' null E'' escape E'"' quote E'"') ENCODING 'UTF8'; 

-- schema changes for gpperfmon needed to complete the creation of the schema

revoke all on database gpperfmon from public;

-- for web ui auth everyone needs connect permissions
grant connect on database gpperfmon to public;

-- END
