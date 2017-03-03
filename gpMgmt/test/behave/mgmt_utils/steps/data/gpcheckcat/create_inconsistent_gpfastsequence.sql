set allow_system_table_mods='dml';
set allow_segment_dml=true;
delete from pg_class where oid in (select objid from gp_fastsequence);
