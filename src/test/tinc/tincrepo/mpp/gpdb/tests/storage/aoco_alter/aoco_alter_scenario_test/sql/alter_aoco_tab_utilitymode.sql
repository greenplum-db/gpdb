\d alter_aoco_tab_utilitymode;
alter table alter_aoco_tab_utilitymode add column add_col1 character varying(35) default 'abc' ;
select count(*) as add_col1  from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='alter_aoco_tab_utilitymode' and attname='add_col1';
