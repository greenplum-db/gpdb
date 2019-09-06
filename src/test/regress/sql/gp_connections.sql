--
-- GPDB internal connection tests
--

-- create a new user
drop user if exists user_disallowed_via_local;
create user user_disallowed_via_local with login;

-- cleanup previous settings if any
\! sed -i '/user_disallowed_via_local/d' $MASTER_DATA_DIRECTORY/pg_hba.conf;
-- allow it to login via the [tcp] protocol
\! echo 'host all user_disallowed_via_local samenet trust' | tee -a $MASTER_DATA_DIRECTORY/pg_hba.conf;
-- disallow it to login via the [local] protocol
\! echo 'local all user_disallowed_via_local reject' | tee -a $MASTER_DATA_DIRECTORY/pg_hba.conf;

-- inform the cluster to reload the settings
\! gpstop -qu;
-- the reloading might not happen immediately, wait for a while
select pg_sleep(2);

-- login via a network address is allowed
\c postgres user_disallowed_via_local localhost

-- now we are the new user
create temp table t1_of_user_disallowed_via_local(c1 int);

-- below query will fork an entry db on master, it will connect via [local],
-- but as it is an internal connection it should still be allowed
select * from t1_of_user_disallowed_via_local, pg_sleep(0);

-- cleanup settings if any
\! sed -i '/user_disallowed_via_local/d' $MASTER_DATA_DIRECTORY/pg_hba.conf;

--
-- Segment connection tests
--

-- We should not be able to directly connect to a primary segment.
SELECT port FROM gp_segment_configuration
			WHERE content <> -1 AND role = 'p'
			LIMIT 1
\gset
\connect - - - :port

-- DON'T PUT ANYTHING BELOW THIS TEST! It'll be ignored since the above \connect
-- fails and exits the script. Add them above, instead.
