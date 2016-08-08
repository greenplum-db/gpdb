create table mdt_test_trunc1 ( a int, b text) distributed by (a);
truncate table  mdt_test_trunc1 cascade;

create table mdt_test_trunc2 ( a int, b text) distributed by (a);
truncate table  mdt_test_trunc2 restrict;

create table mdt_test_trunc3 ( a int, b text) distributed by (a);
truncate mdt_test_trunc3;

drop table mdt_test_trunc1;
drop table mdt_test_trunc2;
drop table mdt_test_trunc3;
