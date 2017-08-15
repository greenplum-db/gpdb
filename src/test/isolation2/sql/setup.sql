-- Helper function, to call either __gp_aoseg_name, or gp_aocsseg_name,
-- dependingwhether the table is row- or column-oriented. This allows us to
-- run the same test queries on both.
--
-- The Python utility that runs this doesn't know about dollar-quoting,
-- and thinks that a ';' at end of line ends the command. The /* in func */
-- comments at the end of each line thwarts that.
CREATE OR REPLACE FUNCTION gp_ao_or_aocs_seg_name(rel text,
  segno OUT integer,
  tupcount OUT bigint,
  modcount OUT bigint,
  formatversion OUT smallint,
  state OUT smallint)
RETURNS SETOF record as $$
declare
  relstorage_var char;	/* in func */
begin	/* in func */
  select relstorage into relstorage_var from pg_class where oid = rel::regclass; /* in func */
  if relstorage_var = 'c' then	/* in func */
    for segno, tupcount, modcount, formatversion, state in SELECT DISTINCT x.segno, x.tupcount, x.modcount, x.formatversion, x.state FROM gp_toolkit.__gp_aocsseg_name(rel) x loop	/* in func */
      return next;	/* in func */
    end loop;	/* in func */
  else	/* in func */
    for segno, tupcount, modcount, formatversion, state in SELECT x.segno, x.tupcount, x.modcount, x.formatversion, x.state FROM gp_toolkit.__gp_aoseg_name(rel) x loop	/* in func */
      return next;	/* in func */
    end loop;	/* in func */
  end if;	/* in func */
end;	/* in func */
$$ LANGUAGE plpgsql;

-- Show locks in master and in segments. Because the number of segments
-- in the cluster depends on configuration, we print only summary information
-- of the locks in segments. If a relation is locked only on one segment,
-- we print that as a special case, but otherwise we just print "n segments",
-- meaning the relation is locked on more than one segment.
create or replace view locktest_master as
select coalesce(
  case when relname like 'pg_toast%index' then 'toast index'
       when relname like 'pg_toast%' then 'toast table'
       when relname like 'pg_aoseg%' then 'aoseg table'
       when relname like 'pg_aovisimap%index' then 'aovisimap index'
       when relname like 'pg_aovisimap%' then 'aovisimap table'
       else relname end, 'dropped table'),
  mode,
  locktype,
  'master'::text as node
from pg_locks l
left outer join pg_class c on ((l.locktype = 'append-only segment file' and l.relation = c.relfilenode) or (l.locktype != 'append-only segment file' and l.relation = c.oid)),
pg_database d
where relation is not null
and l.database = d.oid
and (relname <> 'gp_fault_strategy' and relname != 'locktest_master' or relname is NULL)
and d.datname = current_database()
and l.gp_segment_id = -1
group by l.gp_segment_id, relation, relname, locktype, mode
order by 1, 3, 2;

create or replace view locktest_segments_dist as
select relname,
  mode,
  locktype,
  l.gp_segment_id as node,
  relation
from pg_locks l
left outer join pg_class c on ((l.locktype = 'append-only segment file' and l.relation = c.relfilenode) or (l.locktype != 'append-only segment file' and l.relation = c.oid)),
pg_database d
where relation is not null
and l.database = d.oid
and (relname <> 'gp_fault_strategy' and relname != 'locktest_segments_dist' or relname is NULL)
and d.datname = current_database()
and l.gp_segment_id > -1
group by l.gp_segment_id, relation, relname, locktype, mode;

create or replace view locktest_segments as
SELECT coalesce(
  case when relname like 'pg_toast%index' then 'toast index'
       when relname like 'pg_toast%' then 'toast table'
       when relname like 'pg_aoseg%' then 'aoseg table'
       when relname like 'pg_aovisimap%index' then 'aovisimap index'
       when relname like 'pg_aovisimap%' then 'aovisimap table'
       else relname end, 'dropped table'),
  mode,
  locktype,
  case when count(*) = 1 then '1 segment'
       else 'n segments' end as node
  FROM gp_dist_random('locktest_segments_dist')
  group by relname, relation, mode, locktype;

-- start_ignore
create language plpythonu;
-- end_ignore

CREATE OR REPLACE FUNCTION wait_for_trigger_fault(dbname text, fault text, segno int)
RETURNS bool as $$
    import subprocess 
    import time
    cmd = 'psql %s -c "select gp_inject_fault(\'%s\', \'status\', %d)"' % (dbname, fault, segno)
    for i in range(100):
        cmd_output = subprocess.Popen(cmd, stderr=subprocess.STDOUT, stdout=subprocess.PIPE, shell=True)
        if 'triggered' in cmd_output.stdout.read():
            return True
        time.sleep(0.5)
    return False 
$$ LANGUAGE plpythonu;
