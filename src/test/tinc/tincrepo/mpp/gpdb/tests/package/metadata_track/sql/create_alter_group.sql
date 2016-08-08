CREATE ROLE mdt_grp_role1;
CREATE ROLE mdt_grp_role2;
CREATE GROUP mdt_db_group1 WITH SUPERUSER CREATEDB  INHERIT LOGIN CONNECTION LIMIT  1 ENCRYPTED PASSWORD 'passwd';
CREATE GROUP mdt_db_grp2 WITH NOSUPERUSER NOCREATEDB  NOINHERIT NOLOGIN  UNENCRYPTED PASSWORD 'passwd';
CREATE GROUP mdt_db_grp3 WITH NOCREATEROLE NOCREATEUSER;
CREATE GROUP mdt_db_grp4 WITH CREATEROLE CREATEUSER;
CREATE GROUP mdt_db_grp5 WITH VALID UNTIL '2009-02-13 01:51:15';
CREATE GROUP mdt_db_grp6 WITH IN ROLE mdt_grp_role1; 
CREATE GROUP mdt_db_grp7 WITH IN GROUP mdt_db_group1; 
CREATE GROUP mdt_db_grp8 WITH ROLE mdt_grp_role2;
CREATE GROUP mdt_db_grp9 WITH ADMIN mdt_db_grp8;
CREATE GROUP mdt_db_grp10 WITH USER mdt_db_group1;
CREATE GROUP mdt_db_grp11 SYSID 100 ;
CREATE RESOURCE QUEUE mdt_grp_rsq1 ACTIVE THRESHOLD 1;
CREATE GROUP mdt_db_grp12 RESOURCE QUEUE mdt_grp_rsq1;


CREATE USER mdt_test_user_1;
ALTER GROUP mdt_db_grp7 ADD USER mdt_test_user_1;
ALTER GROUP mdt_db_grp12 ADD USER mdt_test_user_1;
ALTER GROUP mdt_db_grp12 DROP USER mdt_test_user_1;
ALTER GROUP mdt_db_grp11 RENAME TO mdt_new_db_grp11;


drop role mdt_grp_role1;
drop role mdt_grp_role2;
drop group mdt_db_group1;;
drop group mdt_db_grp2;
drop group mdt_db_grp3;
drop group mdt_db_grp4;
drop group mdt_db_grp5;
drop group mdt_db_grp6;
drop group mdt_db_grp7;
drop group mdt_db_grp8;
drop group mdt_db_grp9;
drop group mdt_db_grp10;
drop group mdt_new_db_grp11;
drop group mdt_db_grp12;
drop resource queue mdt_grp_rsq1;
drop user mdt_test_user_1;

