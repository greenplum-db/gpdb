alter table multi_segfile_tab add column a1 boolean DEFAULT false;
select count(*) as a1  from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_tab' and attname='a1';
