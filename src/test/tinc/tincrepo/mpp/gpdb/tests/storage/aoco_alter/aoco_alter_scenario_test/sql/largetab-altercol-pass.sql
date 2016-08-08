alter table large_aoco_table add column col2 int default null;
select count(*) as col2  from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='large_aoco_table' and attname='col2';
