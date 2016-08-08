begin;
alter table heap_table_for_alter set tablespace abort_create_needed_heap_ts2;
drop table heap_table_for_alter;
commit;
