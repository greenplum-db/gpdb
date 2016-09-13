--  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
--  Gpperfmon 4.2 Schema
--
-- Note: Because this file is run as part of upgrade in single user mode we
-- cannot make use of psql escape sequences such as "\c gpperfmon" and every
-- statement must be on a single line.
--
-- Violate the above and you _will_ break upgrade.
--

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






-- TABLE: udp_history ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
-- ctime timestamp(0) not null, 
-- hostname varchar(64) not null, 
-- packets_received bigint,
-- packets_sent bigint,        
-- packets_received_unkown_port int,
-- packet_receive_errors bigint

create table public.udp_history ( ctime timestamp(0) not null, hostname varchar(64) not null, packets_received bigint, packets_sent bigint, packets_received_unkown_port int, packet_receive_errors bigint) with (fillfactor=100) distributed by (ctime) partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));

--- TABLE: udp_now
--   (like udp_history)
create external web table public.udp_now (like public.udp_history) execute 'cat gpperfmon/data/udp_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: udp_tail
--   (like udp_history)
create external web table public.udp_tail (like public.udp_history) execute 'cat gpperfmon/data/udp_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: _udp_tail
--   (like udp_history)
create external web table public._udp_tail (like public.udp_history) execute 'cat gpperfmon/data/_udp_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');





-- TABLE: tcp_history ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
-- ctime timestamp(0) not null, 
-- hostname varchar(64) not null, 
-- segments_received bigint,
-- segments_sent bigint,
-- segments_retransmitted bigint,
-- bad_segments_received int,
-- active_connections int,
-- passive_connections int,
-- failed_connection_attempts int,
-- connections_established int,
-- connection_resets_received int,
-- connection_resets_sent int

create table public.tcp_history ( ctime timestamp(0) not null, hostname varchar(64) not null, segments_received bigint, segments_sent bigint, segments_retransmitted bigint, bad_segments_received int, active_connections int, passive_connections int, failed_connection_attempts int, connections_established int, connection_resets_received int, connection_resets_sent int) with (fillfactor=100) distributed by (ctime) partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month'));

--- TABLE: tcp_now
--   (like tcp_history)
create external web table public.tcp_now (like public.tcp_history) execute 'cat gpperfmon/data/tcp_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: tcp_tail
--   (like tcp_history)
create external web table public.tcp_tail (like public.tcp_history) execute 'cat gpperfmon/data/tcp_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-- TABLE: _tcp_tail
--   (like tcp_history)
create external web table public._tcp_tail (like public.tcp_history) execute 'cat gpperfmon/data/_tcp_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');





-- TABLE: tcp_extended_history ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
-- ctime timestamp(0) not null, 
-- hostname varchar(64) not null, 
-- SyncookiesSent bigint,
-- SyncookiesRecv bigint,
-- SyncookiesFailed bigint,
-- EmbryonicRsts bigint,
-- PruneCalled bigint,
-- RcvPruned bigint,
-- OfoPruned bigint,
-- OutOfWindowIcmps bigint,
-- LockDroppedIcmps bigint,
-- ArpFilter bigint,
-- TW bigint,
-- TWRecycled bigint,
-- TWKilled bigint,
-- PAWSPassive bigint,
-- PAWSActive bigint,
-- PAWSEstab bigint,
-- DelayedACKs bigint,
-- DelayedACKLocked bigint,
-- DelayedACKLost bigint,
-- ListenOverflows bigint,
-- ListenDrops bigint,
-- TCPPrequeued bigint,
-- TCPDirectCopyFromBacklog bigint,
-- TCPDirectCopyFromPrequeue bigint,
-- TCPPrequeueDropped bigint,
-- TCPHPHits bigint,
-- TCPHPHitsToUser bigint,
-- TCPPureAcks  bigint,
-- TCPHPAcks bigint,
-- TCPRenoRecovery bigint,
-- TCPSackRecovery bigint,
-- TCPSACKReneging bigint,
-- TCPFACKReorder bigint,
-- TCPSACKReorder bigint,
-- TCPRenoReorder bigint,
-- TCPTSReorder bigint,
-- TCPFullUndo bigint,
-- TCPPartialUndo bigint,
-- TCPDSACKUndo bigint,
-- TCPLossUndo bigint,
-- TCPLoss bigint,
-- TCPLostRetransmit bigint,
-- TCPRenoFailures bigint,
-- TCPSackFailures bigint,
-- TCPLossFailures bigint,
-- TCPFastRetrans bigint,
-- TCPForwardRetrans bigint,
-- TCPSlowStartRetrans bigint,
-- TCPTimeouts bigint,
-- TCPRenoRecoveryFail bigint,
-- TCPSackRecoveryFail bigint,
-- TCPSchedulerFailed bigint,
-- TCPRcvCollapsed bigint,
-- TCPDSACKOldSent bigint,
-- TCPDSACKOfoSent bigint,
-- TCPDSACKRecv bigint,
-- TCPDSACKOfoRecv bigint,
-- TCPAbortOnSyn bigint,
-- TCPAbortOnData bigint,
-- TCPAbortOnClose bigint,
-- TCPAbortOnMemory bigint,
-- TCPAbortOnTimeout bigint,
-- TCPAbortOnLinger bigint,
-- TCPAbortFailed bigint,
-- TCPMemoryPressures bigint

create table public.tcp_extended_history ( ctime timestamp(0) not null, hostname varchar(64) not null, SyncookiesSent bigint, SyncookiesRecv bigint, SyncookiesFailed bigint, EmbryonicRsts bigint, PruneCalled bigint, RcvPruned bigint, OfoPruned bigint, OutOfWindowIcmps bigint, LockDroppedIcmps bigint, ArpFilter bigint, TW bigint, TWRecycled bigint, TWKilled bigint, PAWSPassive bigint, PAWSActive bigint, PAWSEstab bigint, DelayedACKs bigint, DelayedACKLocked bigint, DelayedACKLost bigint, ListenOverflows bigint, ListenDrops bigint, TCPPrequeued bigint, TCPDirectCopyFromBacklog bigint, TCPDirectCopyFromPrequeue bigint, TCPPrequeueDropped bigint, TCPHPHits bigint, TCPHPHitsToUser bigint, TCPPureAcks  bigint, TCPHPAcks bigint, TCPRenoRecovery bigint, TCPSackRecovery bigint, TCPSACKReneging bigint, TCPFACKReorder bigint, TCPSACKReorder bigint, TCPRenoReorder bigint, TCPTSReorder bigint, TCPFullUndo bigint, TCPPartialUndo bigint, TCPDSACKUndo bigint, TCPLossUndo bigint, TCPLoss bigint, TCPLostRetransmit bigint, TCPRenoFailures bigint, TCPSackFailures bigint, TCPLossFailures bigint, TCPFastRetrans bigint, TCPForwardRetrans bigint, TCPSlowStartRetrans bigint, TCPTimeouts bigint, TCPRenoRecoveryFail bigint, TCPSackRecoveryFail bigint, TCPSchedulerFailed bigint, TCPRcvCollapsed bigint, TCPDSACKOldSent bigint, TCPDSACKOfoSent bigint, TCPDSACKRecv bigint, TCPDSACKOfoRecv bigint, TCPAbortOnSyn bigint, TCPAbortOnData bigint, TCPAbortOnClose bigint, TCPAbortOnMemory bigint, TCPAbortOnTimeout bigint, TCPAbortOnLinger bigint, TCPAbortFailed bigint, TCPMemoryPressures bigint) with (fillfactor=100) distributed by (ctime) partition by range (ctime)(start (date '2010-01-01') end (date '2010-02-01') EVERY (interval '1 month')); 

--- TABLE: tcp_extended_now 
--   (like tcp_extended_history) 
create external web table public.tcp_extended_now (like public.tcp_extended_history) execute 'cat gpperfmon/data/tcp_extended_now.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null'); 

-- TABLE: tcp_extended_tail 
--   (like tcp_extended_history) 
create external web table public.tcp_extended_tail (like public.tcp_extended_history) execute 'cat gpperfmon/data/tcp_extended_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null'); 

-- TABLE: _tcp_extended_tail
--   (like tcp_extended_history)
create external web table public._tcp_extended_tail (like public.tcp_extended_history) execute 'cat gpperfmon/data/_tcp_extended_tail.dat 2> /dev/null || true' on master format 'text' (delimiter '|' NULL as 'null');

-------------- MODIFY QUERY TABLE ----------------------------------

-- add new columns to queries table
alter external table public.queries_now add column application_name varchar(64), add column rsqname varchar(64), add column rqppriority varchar(16);
alter external table public.queries_tail add column application_name varchar(64), add column rsqname varchar(64), add column rqppriority varchar(16);
alter external table public._queries_tail add column application_name varchar(64), add column rsqname varchar(64), add column rqppriority varchar(16);
alter table public.queries_history add column application_name varchar(64), add column rsqname varchar(64), add column rqppriority varchar(16);

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

-- END
