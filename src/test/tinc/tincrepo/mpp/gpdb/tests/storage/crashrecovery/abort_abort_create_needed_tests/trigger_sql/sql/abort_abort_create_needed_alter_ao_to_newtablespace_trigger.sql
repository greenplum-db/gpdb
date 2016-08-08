begin;
alter table ao_table_for_alter set tablespace abort_create_needed_ao_ts2;
drop table ao_table_for_alter;
commit;
