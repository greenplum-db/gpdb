-- start_ignore
\! gpconfig -c autovacuum -v off;
\! gpstop -au;
-- end_ignore

-- Check if autovacuum is enabled/disabled by inspecting the av launcher.
CREATE or REPLACE FUNCTION check_autovacuum (enabled boolean) RETURNS bool AS
$$
declare
	retries int;
	expected_count int;
begin
	retries := 1200;
	if enabled then
		/* (1 for each primary and 1 for the coordinator) */
		expected_count := 4;
	else 
		expected_count := 0;
	end if;
	loop
		if (select count(*) = expected_count from gp_stat_activity
			where backend_type = 'autovacuum launcher') then
			return true;
		end if;
		if retries <= 0 then
			return false;
		end if;
		perform pg_sleep(0.1);
		retries := retries - 1;
	end loop;
end;
$$
language plpgsql;

-- Impose a stronger exit criteria for this test:
-- the AV launcher has shut down (and by extension the workers) following the config change.
-- This is done to ensure tests in the suite immediately following this one are run under the right conditions.
select check_autovacuum(false);
