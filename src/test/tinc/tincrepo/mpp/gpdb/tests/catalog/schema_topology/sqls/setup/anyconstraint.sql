-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology

--start_ignore
drop view if exists relconstraint cascade;
drop view if exists ptable cascade;
drop view if exists part_closure cascade;
drop view if exists anyconstraint cascade;
--end_ignore

create view relconstraint
    (
        relid,
        conid,
        conname,
        contype,
        consrc,
        condef,
        indexid
    )
    as
select  
    r.oid::regclass as relid, 
    c.oid as conid,
    c.conname,
    c.contype,
    c.consrc,
    pg_get_constraintdef(c.oid) as condef,
    d.objid::regclass as indexid
from 
    (
        pg_class r
			join
			pg_constraint c
			on 
			    r.oid = c.conrelid
			    and r.relkind = 'r'
	)
	    left join
	    pg_depend d
	    on 
	        d.refobjid = c.oid
	        and d.classid = 'pg_class'::regclass
	        and d.refclassid = 'pg_constraint'::regclass
	        and d.deptype = 'i';

create view ptable
    (
        tableid,
        tabledepth
    )
    as
    select 
        parrelid::regclass,
        max(parlevel)+1
    from
        pg_partition
    group by parrelid;

create view part_closure
    (
        tableid,
        tabledepth,
        partid,
        partdepth,
        partordinal,
        partstatus
    )
    as
    select 
        tableid,  
        tabledepth,
        tableid::regclass partid, 
        0 as partdepth, 
        0 as partordinal,
        'r'::char as partstatus
    from 
        ptable
    union all
    select 
        parrelid::regclass as tableid,  
        t.tabledepth as tabledepth,
        r.parchildrelid::regclass partid, 
        p.parlevel + 1 as partdepth, 
        r.parruleord as partordinal,
        case
            when t.tabledepth = p.parlevel + 1 then 'l'::char
            else 'i'::char
        end as partstatus
    from 
        pg_partition p, 
        pg_partition_rule r,
        ptable t
    where 
        p.oid = r.paroid
        and not p.paristemplate
        and p.parrelid = t.tableid;

create view anyconstraint
    (
        tableid,
        partid,
        relid,
        conid,
        conname,
        contype,
        consrc,
        condef,
        indexid
    )
    as
     select 
         coalesce(p.tableid, c.relid) as tableid,
         p.partid,
         c.relid,
         c.conid,
         c.conname,
         c.contype,
         c.consrc,
         c.condef,
         c.indexid
    from 
        relconstraint c
            left join 
        part_closure p
            on relid = partid;
