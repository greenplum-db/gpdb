--  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
--  Gpperfmon 4.1 Schema
--
-- Note: Because this file is run as part of upgrade in single user mode we
-- cannot make use of psql escape sequences such as "\c gpperfmon" and every
-- statement must be on a single line.
--
-- Violate the above and you _will_ break upgrade.
--

-- TABLE: health_history
--   ctime                      event time
--   hostname                   hostname of system this metric belongs to
--   symptom_code               code for this type of event
--   detailed_symptom_code      finer grain symptom code
--   description                text description of event type
--   status                     indication of status at time of event
--   message                    text message associated with this event
create table public.health_history (ctime timestamp(0) not null, hostname varchar(64) not null, symptom_code int not null, detailed_symptom_code int not null, description text not null, status text not null, message text not null) distributed by (ctime) partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));

-- TABLE: filerep_history
--   ctime                      		time of measurement
--   primary_measurement_microsec 		elapsed seconds for primary measurements
--   mirror_measurement_microsec 		elapsed seconds for mirror measurements
--   primary_hostname           		key -- primary segment
--   primary_port           			key -- primary segment
--   mirror_hostname            		key -- mirror segment
--   mirror_port           			key -- mirror segment
--   primary_write_syscall_bytes_avg		write system calls on primary
--   primary_write_syscall_byte_max
--   primary_write_syscall_microsecs_avg
--   primary_write_syscall_microsecs_max
--   primary_write_syscall_per_sec 
--   primary_fsync_syscall_microsec_avg		fysnc system calls on primary
--   primary_fsync_syscall_microsec_max
--   primary_fsync_syscall_per_sec
--   primary_write_shmem_bytes_avg		putting write messages into shared memory on primary
--   primary_write_shmem_bytes_max
--   primary_write_shmem_microsec_avg
--   primary_write_shmem_microsec_max
--   primary_write_shmem_per_sec 
--   primary_fsync_shmem_microsec_avg		putting fsync messages into shared memory on primary
--   primary_fsync_shmem_microsec_max
--   primary_fsync_shmem_per_sec
--   primary_roundtrip_fsync_msg_microsec_avg	Roundtrip from sending fsync message to mirror to get ack back on primary
--   primary_roundtrip_fsync_msg_microsec_max
--   primary_roundtrip_fsync_msg_per_sec
--   primary_roundtrip_test_msg_microsec_avg	Roundtrip from sending test message to mirror to getting back on primary
--   primary_roundtrip_test_msg_microsec_max
--   primary_roundtrip_test_msg_per_sec
--   mirror_write_syscall_size_avg		write system call on mirror
--   mirror_write_syscall_size_max
--   mirror_write_syscall_microsec_avg
--   mirror_write_syscall_microsec_max
--   mirror_write_syscall_per_sec
--   mirror_fsync_syscall_microsec_avg 		fsync system call on mirror
--   mirror_fsync_syscall_microsec_max
--   mirror_fsync_syscall_per_sec
create table public.filerep_history (ctime timestamp(0) not null, primary_measurement_microsec bigint not null, mirror_measurement_microsec bigint not null, primary_hostname varchar(64) not null, primary_port int not null, mirror_hostname varchar(64) not null, mirror_port int not null, primary_write_syscall_bytes_avg	bigint not null, primary_write_syscall_byte_max bigint not null, primary_write_syscall_microsecs_avg bigint not null, primary_write_syscall_microsecs_max bigint not null, primary_write_syscall_per_sec float not null, primary_fsync_syscall_microsec_avg bigint not null, primary_fsync_syscall_microsec_max bigint not null, primary_fsync_syscall_per_sec float not null, primary_write_shmem_bytes_avg bigint not null, primary_write_shmem_bytes_max bigint not null, primary_write_shmem_microsec_avg bigint not null, primary_write_shmem_microsec_max bigint not null, primary_write_shmem_per_sec float not null, primary_fsync_shmem_microsec_avg bigint not null, primary_fsync_shmem_microsec_max bigint not null, primary_fsync_shmem_per_sec float not null, primary_roundtrip_fsync_msg_microsec_avg bigint not null, primary_roundtrip_fsync_msg_microsec_max bigint not null, primary_roundtrip_fsync_msg_per_sec float not null, primary_roundtrip_test_msg_microsec_avg bigint not null, primary_roundtrip_test_msg_microsec_max bigint not null, primary_roundtrip_test_msg_per_sec float not null, mirror_write_syscall_size_avg bigint not null, mirror_write_syscall_size_max bigint not null, mirror_write_syscall_microsec_avg bigint not null, mirror_write_syscall_microsec_max bigint not null, mirror_write_syscall_per_sec float not null, mirror_fsync_syscall_microsec_avg bigint not null, mirror_fsync_syscall_microsec_max bigint not null, mirror_fsync_syscall_per_sec float not null) with (fillfactor=100) distributed by (ctime) partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));

--- TABLE: filerep_now
--   (like filerep_history)
create external web table public.filerep_now (like public.filerep_history) execute 'cat gpperfmon/data/filerep_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: diskpace_tail
--   (like filerep_history)
create external web table public.filerep_tail (like public.filerep_history) execute 'cat gpperfmon/data/filerep_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: _filerep_tail
--   (like filerep_history)
create external web table public._filerep_tail (like public.filerep_history) execute 'cat gpperfmon/data/_filerep_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');


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
