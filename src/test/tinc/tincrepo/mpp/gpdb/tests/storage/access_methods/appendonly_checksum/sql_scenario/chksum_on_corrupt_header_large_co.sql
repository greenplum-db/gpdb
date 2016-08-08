-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Large Content Header
drop table if exists chksum_on_corrupt_header_large_co;
create table chksum_on_corrupt_header_large_co(comment bytea ) with (appendonly=true, orientation=column, checksum=true) DISTRIBUTED RANDOMLY;
insert into chksum_on_corrupt_header_large_co select ("decode"(repeat('a',33554432),'escape')) from generate_series(1,8)  ;
