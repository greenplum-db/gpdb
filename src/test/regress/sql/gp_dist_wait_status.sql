-- Check gp_dist_wait_status not failing within a transaction

BEGIN;
	select * from gp_dist_wait_status();
COMMIT;
