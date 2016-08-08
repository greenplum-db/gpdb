-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Create user, alter user with deny rules

CREATE USER ck_sync1_tmuser1 WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';
CREATE USER ck_sync1_tmuser2 WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';
CREATE USER ck_sync1_tmuser3 WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';
CREATE USER ck_sync1_tmuser4 WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';
CREATE USER ck_sync1_tmuser5 WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';

ALTER USER sync1_tmuser2 DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';
ALTER USER ck_sync1_tmuser1 DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';
