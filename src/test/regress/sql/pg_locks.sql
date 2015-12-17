-- check if segments lock status is collected for Create-stmt
-- start_ignore
create table pg_locks_t1 as SELECT l.locktype, l.database, l.relation, l.page, l.tuple, l.transactionid, l.classid, l.objid, l.objsubid, l.transaction, l.pid, l.mode, l.granted, l.mppsessionid, l.mppiswriter, l.gp_segment_id as gpid FROM pg_lock_status() l(locktype text, database oid, relation oid, page integer, tuple smallint, transactionid xid, classid oid, objid oid, objsubid smallint, transaction xid, pid integer, mode text, granted boolean, mppsessionid integer, mppiswriter boolean, gp_segment_id integer);
-- end_ignore

select count(*) > 0 result from pg_locks_t1 where pg_locks_t1.gpid=0 or pg_locks_t1.gpid=-1;

-- check if segments lock status is collected for Insert-stmt
-- start_ignore
create table pg_locks_t2 (like pg_locks_t1);
insert into pg_locks_t2 select * from pg_locks;
-- end_ignore

select count(*) > 0 from pg_locks_t2 where pg_locks_t2.gpid = 0 or pg_locks_t2.gpid = -1 group by gpid;

-- check if pg_locks work with entry-db-locus
select count(*) > 0 from pg_locks l, pg_class where l.gp_segment_id = 0 or l.gp_segment_id = -1;

select count(*) > 0 from pg_locks l, generate_series(1, 2) where l.gp_segment_id = -1 or l.gp_segment_id = 0;

-- check if pg_locks work with partitioned-locus relation
select count(*) > 0 from pg_locks l, pg_locks_t1 where l.gp_segment_id = pg_locks_t1.gpid and pg_locks_t1.gpid = 0;
select count(*) > 0 from pg_locks l, pg_locks_t1 where l.gp_segment_id = pg_locks_t1.gpid and pg_locks_t1.gpid = -1;

-- start_ignore
update pg_locks_t2 set gpid = 1000 from (select gp_segment_id from pg_locks) l where l.gp_segment_id = 0;
update pg_locks_t2 set gpid = 2000 from (select gp_segment_id from pg_locks) l where l.gp_segment_id = -1;
-- end_ignore

select count(gpid) > 0 from pg_locks_t2 where gpid = 1000 or gpid = 2000 group by gpid;

-- check if union command works 
select count(*) > 0 from (select * from pg_locks union all (select * from pg_locks_t1)) sub;

-- test partition table
-- start_ignore
create table pg_lock_p1 (like pg_locks_t1) partition by list (gpid) ( partition master values (-1), partition seg1 values (0), partition seg2 values (1), default partition other);

insert into pg_lock_p1 select * from pg_locks;
-- end_ignore

select count(*) > 0 result from pg_lock_p1 where pg_lock_p1.gpid=0;

-- start_ignore
delete from pg_lock_p1 where pg_lock_p1.gpid in (select gp_segment_id from pg_locks);
-- end_ignore
select count(*) = 0 from pg_lock_p1;

-- check if delete and update command works 
-- start_ignore
delete from pg_locks_t1 where pg_locks_t1.gpid in (select gp_segment_id from pg_locks);
-- end_ignore
select count(*) = 0 from pg_locks_t1;

