create user mdt_user1 with superuser;
create group mdt_group1 with superuser;

create table mdt_test_table1 ( a int, b text) distributed by (a);

create table mdt_test_table2 ( a int, b text) distributed by (a);
grant delete, references, trigger on  mdt_test_table2 to group mdt_group1 with grant option;

create table mdt_test_table3 ( a int, b text) distributed by (a);
grant all on mdt_test_table3 to mdt_user1 with grant option;

create table mdt_test_table4 ( a int, b text) distributed by (a);
grant all privileges on mdt_test_table4 to mdt_user1 with grant option;

create table mdt_test_table5 ( a int, b text) distributed by (a);
grant all privileges on mdt_test_table5 to group mdt_group1 with grant option;

create table mdt_test_table6 ( a int, b text) distributed by (a);
create table mdt_test_table7 ( a int, b text) distributed by (a);
grant all on mdt_test_table6,mdt_test_table7 to public ;


drop table mdt_test_table1;
drop table mdt_test_table2;
drop table mdt_test_table3;
drop table mdt_test_table4;
drop table mdt_test_table5;
drop table mdt_test_table6;
drop table mdt_test_table7;
drop user mdt_user1;
drop group mdt_group1;
