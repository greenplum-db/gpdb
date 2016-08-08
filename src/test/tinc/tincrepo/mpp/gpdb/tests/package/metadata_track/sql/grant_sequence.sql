create user mdt_user1 with superuser;
create group mdt_group1 with superuser;

CREATE TABLE mdt_test_tbl_seq ( col1 int, col2 int) DISTRIBUTED RANDOMLY;
INSERT INTO mdt_test_tbl_seq values (generate_series(1,100),generate_series(1,100));
CREATE SEQUENCE mdt_db_seq9 START 101 OWNED BY mdt_test_tbl_seq.col1;
grant usage on sequence mdt_db_seq9 to mdt_user1 with grant option;

CREATE SEQUENCE  mdt_db_seq10 START WITH 101;

CREATE SEQUENCE  mdt_db_seq11 START WITH 101;
grant all on sequence mdt_db_seq11 to public;

CREATE SEQUENCE  mdt_db_seq12 START WITH 101;
grant all privileges on sequence mdt_db_seq12 to public;


drop table mdt_test_tbl_seq;
drop sequence  mdt_db_seq10 ;
drop sequence  mdt_db_seq11 ;
drop sequence  mdt_db_seq12 ;
drop user mdt_user1;
drop group mdt_group1;

