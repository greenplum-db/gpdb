CREATE RESOURCE QUEUE db_resque_new1 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;
CREATE RESOURCE QUEUE db_resque_new2 COST THRESHOLD 3000.00 OVERCOMMIT;
CREATE RESOURCE QUEUE db_resque_new3 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;
CREATE RESOURCE QUEUE db_resque_new4 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;
CREATE RESOURCE QUEUE db_resque_new5 COST THRESHOLD 3000.00 OVERCOMMIT;
CREATE RESOURCE QUEUE db_resque_new6 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;
CREATE RESOURCE QUEUE db_resque_new7 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;
CREATE RESOURCE QUEUE db_resque_new8 ACTIVE THRESHOLD 2 COST THRESHOLD 2000.00;

ALTER RESOURCE QUEUE db_resque_new1 with(priority=high, max_cost=300) without (cost_overcommit=false) ;
ALTER RESOURCE QUEUE db_resque_new2 without (cost_overcommit=false);
ALTER RESOURCE QUEUE db_resque_new3 with(priority=low); 
ALTER RESOURCE QUEUE db_resque_new4 with(MAX_COST=3.0);
ALTER RESOURCE QUEUE db_resque_new5 with(MIN_COST=1.0);
ALTER RESOURCE QUEUE db_resque_new6 with(ACTIVE_STATEMENTS=3);
ALTER RESOURCE QUEUE db_resque_new7 without(cost_overcommit=false);
ALTER RESOURCE QUEUE db_resque_new8 without(ACTIVE_STATEMENTS=2);

drop resource queue db_resque_new1;
drop resource queue db_resque_new2;
drop resource queue db_resque_new3;
drop resource queue db_resque_new4;
drop resource queue db_resque_new5;
drop resource queue db_resque_new6;
drop resource queue db_resque_new7;
drop resource queue db_resque_new8;

