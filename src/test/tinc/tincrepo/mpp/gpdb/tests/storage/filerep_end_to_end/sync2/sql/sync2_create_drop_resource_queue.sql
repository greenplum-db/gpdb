-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE RESOURCE QUEUE sync2_resque1 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;
CREATE RESOURCE QUEUE sync2_resque2 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;

DROP RESOURCE QUEUE sync1_resque7;
DROP RESOURCE QUEUE ck_sync1_resque6;
DROP RESOURCE QUEUE ct_resque4;
DROP RESOURCE QUEUE resync_resque2;
DROP RESOURCE QUEUE sync2_resque1;
