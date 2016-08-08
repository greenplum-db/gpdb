begin;
alter table co_table_for_alter set tablespace abort_create_needed_co_ts2;
drop table co_table_for_alter;
commit;
