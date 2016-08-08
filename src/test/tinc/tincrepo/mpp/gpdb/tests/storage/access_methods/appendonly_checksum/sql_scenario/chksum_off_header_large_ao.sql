-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Large Content Header
drop table if exists chksum_off_header_large_ao;
create table chksum_off_header_large_ao(comment bytea ) with (appendonly=true, checksum=true) DISTRIBUTED RANDOMLY;
insert into chksum_off_header_large_ao select ("decode"(repeat('a',33554432),'escape')) from generate_series(1,16)  ;
