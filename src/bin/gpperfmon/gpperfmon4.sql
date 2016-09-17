--  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
--  Gpperfmon 4.x Schema
--
-- Note: Because this file is run as part of upgrade in single user mode we
-- cannot make use of psql escape sequences such as "\c gpperfmon" and every
-- statement must be on a single line.
--
-- Violate the above and you _will_ break upgrade.
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

-- END
