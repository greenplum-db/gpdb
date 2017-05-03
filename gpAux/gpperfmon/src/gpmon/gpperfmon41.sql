--  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
--  Gpperfmon 4.1 Schema
--
-- Note: Because this file is run as part of upgrade in single user mode we
-- cannot make use of psql escape sequences such as "\c gpperfmon" and every
-- statement must be on a single line.
--
-- Violate the above and you _will_ break upgrade.
--

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

-- END
