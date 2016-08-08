-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Drops databases, Roles and other objects

DROP database IF EXISTS create_objects_db;
DROP database IF EXISTS create_table_db;
DROP database IF EXISTS db_tobe_vacuum;
DROP database IF EXISTS db_tobe_vacuum_analyze;
DROP database IF EXISTS db_test;
DROP database IF EXISTS ao_db;
DROP database IF EXISTS partition_db;
DROP database IF EXISTS check_oid_relfilenode_db;
DROP database IF EXISTS vacuum_data_db;
DROP database IF EXISTS unvacuum_data_db;
DROP database IF EXISTS "TEST_DB";
DROP database IF EXISTS test_db;
DROP database IF EXISTS "TEST_db";
DROP database IF EXISTS test_db1;
DROP database IF EXISTS alter_table_db;
DROP database IF EXISTS cancel_trans;
DROP database IF EXISTS ao_table_drop_col1;
DROP database IF EXISTS ao_table_drop_col2;
DROP database IF EXISTS ao_table_drop_col3;
DROP database IF EXISTS co_db;
DROP database IF EXISTS co_table_drop_col3;

DROP user IF EXISTS "MAIN_USER";
DROP user IF EXISTS "sub_user" ;
DROP user IF EXISTS "SUB_user_1";
DROP user IF EXISTS "user123" ;

DROP role  IF EXISTS user_1;
DROP role IF EXISTS "ISO";
DROP role IF EXISTS "geography" ;
DROP role IF EXISTS "ISO_ro_1";
DROP role IF EXISTS "iso123" ;

DROP GROUP IF EXISTS prachgrp;

-- Drop different Objects related to filespace test
DROP TABLE IF EXISTS sch_fsts_test_part1;
DROP TABLE IF EXISTS sch_fsts_table;
DROP TABLE IF EXISTS sch_fsts_table_ao;
DROP TABLE IF EXISTS sch_fsts_table_co;
DROP TABLE IF EXISTS sch_fsts_part_ao_co;
DROP TABLE IF EXISTS fsts_heap_btree;
DROP TABLE IF EXISTS fsts_heap_bitmap;
DROP TABLE IF EXISTS fsts_heap_unique;
DROP TABLE IF EXISTS fsts_heap_gist;
DROP TABLE IF EXISTS fsts_AO_btree;
DROP TABLE IF EXISTS fsts_AO_bitmap;
DROP TABLE IF EXISTS fsts_CO_btree;
DROP TABLE IF EXISTS fsts_CO_bitmap;
DROP TABLE IF EXISTS fsts_part_range_A6;
DROP TABLE IF EXISTS fsts_part_range;
DROP TABLE IF EXISTS sch_fsts_films;
DROP TABLE IF EXISTS sch_fsts_child_table;
DROP TABLE IF EXISTS sch_fsts_parent_table;
DROP database IF EXISTS fsts_db;
DROP database IF EXISTS sch_fsts_db_name1;
DROP role IF EXISTS sch_fsts_db_owner1;
DROP role IF EXISTS sch_fsts_db_owner2;
DROP tablespace IF EXISTS ts_sch1 ;
DROP tablespace IF EXISTS ts_sch2 ;
DROP tablespace IF EXISTS ts_sch3 ;
DROP tablespace IF EXISTS ts_sch4 ;
DROP tablespace IF EXISTS ts_sch5 ;
DROP tablespace IF EXISTS ts_sch6 ;

\c template1
DROP database IF EXISTS heap_table_drop_col3;
DROP ROLE IF EXISTS admin ;

\c db_test_bed
DROP RESOURCE QUEUE db_resque2;
DROP RESOURCE QUEUE DB_RESque3;
DROP RESOURCE QUEUE DB_RESQUE4;

DROP ROLE IF EXISTS db_role1;
DROP ROLE IF EXISTS db_role2;
DROP ROLE IF EXISTS db_role3;
DROP ROLE IF EXISTS db_role4;
DROP ROLE IF EXISTS db_role5;
DROP ROLE IF EXISTS db_role6;
DROP ROLE IF EXISTS db_role7;
DROP ROLE IF EXISTS new_role8;
DROP ROLE IF EXISTS db_role9;
DROP ROLE IF EXISTS db_role10;
DROP ROLE IF EXISTS db_role11;
DROP ROLE IF EXISTS db_role12;
DROP GROUP IF EXISTS db_grp1;
DROP SCHEMA IF EXISTS db_schema1;

DROP GROUP IF EXISTS db_user_grp1;
DROP ROLE IF EXISTS db_user1;
DROP ROLE IF EXISTS db_user2;
DROP ROLE IF EXISTS db_user3;
DROP ROLE IF EXISTS db_user4;
DROP ROLE IF EXISTS db_user5;
DROP ROLE IF EXISTS db_user6;
DROP ROLE IF EXISTS db_user7;
DROP ROLE IF EXISTS new_user8;
DROP ROLE IF EXISTS db_user9;
DROP ROLE IF EXISTS db_user10;
DROP ROLE IF EXISTS db_user11;
DROP ROLE IF EXISTS db_user12;
DROP SCHEMA IF EXISTS db_schema2;
DROP RESOURCE QUEUE resqueu3;
DROP RESOURCE QUEUE resqueu4;

DROP DATABASE IF EXISTS db_schema_test;
DROP USER IF EXISTS db_user13 ;

DROP ROLE IF EXISTS db_owner1;
DROP DATABASE IF EXISTS db_name1;
DROP ROLE IF EXISTS db_owner2;
DROP SCHEMA IF EXISTS myschema;

DROP GROUP IF EXISTS db_group1; 
DROP GROUP IF EXISTS db_grp2;
DROP GROUP IF EXISTS db_grp3;
DROP GROUP IF EXISTS db_grp4;
DROP GROUP IF EXISTS db_grp5;
DROP GROUP IF EXISTS db_grp6;
DROP GROUP IF EXISTS db_grp7;
DROP GROUP IF EXISTS db_grp8;
DROP GROUP IF EXISTS db_grp9;
DROP GROUP IF EXISTS db_grp10;
DROP GROUP IF EXISTS db_grp11;
DROP GROUP IF EXISTS db_grp12;
DROP ROLE IF EXISTS grp_role1;
DROP ROLE IF EXISTS grp_role2;
DROP USER IF EXISTS test_user_1;
DROP RESOURCE QUEUE grp_rsq1 ;

DROP RESOURCE QUEUE db_resque1;

DROP TABLE IF EXISTS test_tbl;
DROP SEQUENCE  IF EXISTS db_seq3;
DROP SEQUENCE  IF EXISTS db_seq4;
DROP SEQUENCE  IF EXISTS db_seq5;
DROP SEQUENCE  IF EXISTS db_seq7;
DROP SCHEMA IF EXISTS db_schema9 CASCADE;

DROP TABLE IF EXISTS test_emp CASCADE;

DROP LANGUAGE IF EXISTS plpgsql CASCADE;

DROP DOMAIN IF EXISTS domain_us_zip_code;
DROP DOMAIN IF EXISTS domain_1;
DROP DOMAIN IF EXISTS domain_2;
DROP SCHEMA IF EXISTS domain_schema CASCADE;
DROP ROLE IF EXISTS domain_owner;

DROP FUNCTION IF EXISTS scube_accum(numeric, numeric)CASCADE;
DROP FUNCTION IF EXISTS pre_accum(numeric, numeric)CASCADE;
DROP FUNCTION IF EXISTS final_accum(numeric)CASCADE;
DROP ROLE IF EXISTS agg_owner;
DROP SCHEMA IF EXISTS agg_schema;
DROP FUNCTION IF EXISTS add(integer, integer) CASCADE; 
DROP ROLE IF EXISTS func_role ;
DROP SCHEMA IF EXISTS func_schema; 

DROP VIEW IF EXISTS emp_view CASCADE;
DROP TABLE IF EXISTS test_emp_view;

DROP ROLE IF EXISTS sally;
DROP ROLE IF EXISTS ron;
DROP ROLE IF EXISTS ken;

\c db_test_bed 
DROP TABLE IF EXISTS sch_tbint CASCADE;
DROP TABLE IF EXISTS sch_tchar CASCADE;
DROP TABLE IF EXISTS sch_tclob CASCADE;
DROP TABLE IF EXISTS sch_tversion CASCADE;
DROP TABLE IF EXISTS sch_tjoin2 CASCADE;
DROP TABLE IF EXISTS sch_T29 CASCADE;
DROP TABLE IF EXISTS sch_T33 CASCADE;
DROP TABLE IF EXISTS sch_T43 CASCADE;
DROP VIEW IF EXISTS sch_srf_view1;
DROP VIEW IF EXISTS sch_fn_view2;
DROP FUNCTION IF EXISTS sch_multiply(integer,integer);

\c template1
DROP DATABASE IF EXISTS db_test_bed;
DROP ROLE IF EXISTS admin ;

\c gptest
DROP TABLE IF EXISTS region_hybrid_part;
DROP TABLE IF EXISTS nation_hybrid_part;
DROP TABLE IF EXISTS part_hybrid_part;
DROP TABLE IF EXISTS partsupp_hybrid_part;
DROP TABLE IF EXISTS supplier_hybrid_part;
DROP TABLE IF EXISTS orders_hybrid_part;
DROP TABLE IF EXISTS lineitem_hybrid_part;
DROP TABLE IF EXISTS customer_hybrid_part;

