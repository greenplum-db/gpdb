-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description This ddl consists of create, alter resource queues

\c db_test_bed
-- start_ignore
DROP RESOURCE QUEUE create_alter_resource_queue_db_resque1;
DROP RESOURCE QUEUE create_alter_resource_queue_db_resque2;
DROP RESOURCE QUEUE create_alter_resource_queue_db_resque3;
DROP RESOURCE QUEUE create_alter_resource_queue_db_resque4;
-- end_ignore
CREATE RESOURCE QUEUE create_alter_resource_queue_db_resque1 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;
CREATE RESOURCE QUEUE create_alter_resource_queue_db_resque2 COST THRESHOLD 3000.00 OVERCOMMIT;
CREATE RESOURCE QUEUE create_alter_resource_queue_db_resque3 COST THRESHOLD 2000.0 NOOVERCOMMIT;
CREATE RESOURCE QUEUE create_alter_resource_queue_db_resque4 ACTIVE THRESHOLD 2  IGNORE THRESHOLD  1000.0;

ALTER RESOURCE QUEUE create_alter_resource_queue_db_resque1 ACTIVE THRESHOLD 3 COST THRESHOLD 1000.00;
ALTER RESOURCE QUEUE create_alter_resource_queue_db_resque2 COST THRESHOLD 300.00 NOOVERCOMMIT;
ALTER RESOURCE QUEUE create_alter_resource_queue_db_resque3 COST THRESHOLD 200.0 OVERCOMMIT;
ALTER RESOURCE QUEUE create_alter_resource_queue_db_resque4 ACTIVE THRESHOLD 5  IGNORE THRESHOLD  500.0;

