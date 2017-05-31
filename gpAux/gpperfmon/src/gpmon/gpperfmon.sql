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
