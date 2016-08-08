begin;
CREATE TABLESPACE abort_create_needed_ts1 FILESPACE filespace_test_a;
drop tablespace abort_create_needed_ts1;
commit;
