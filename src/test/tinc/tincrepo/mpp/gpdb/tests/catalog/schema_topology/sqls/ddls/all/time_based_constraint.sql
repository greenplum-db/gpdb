-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Time based constraints

DROP USER IF EXISTS testuser;
CREATE USER testuser WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';;

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'testuser'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '02:00:00' AND DAY 'Monday' TIME '02:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '03:00:00' AND DAY 'Monday' TIME '03:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '04:00:00' AND DAY 'Monday' TIME '04:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '05:00:00' AND DAY 'Monday' TIME '05:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '06:00:00' AND DAY 'Monday' TIME '06:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '07:00:00' AND DAY 'Monday' TIME '07:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '08:00:00' AND DAY 'Monday' TIME '08:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '09:00:00' AND DAY 'Monday' TIME '09:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '10:00:00' AND DAY 'Monday' TIME '10:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '11:00:00' AND DAY 'Monday' TIME '11:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '12:00:00' AND DAY 'Monday' TIME '12:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '13:00:00' AND DAY 'Monday' TIME '13:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '14:00:00' AND DAY 'Monday' TIME '14:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '15:00:00' AND DAY 'Monday' TIME '15:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '16:00:00' AND DAY 'Monday' TIME '16:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '17:00:00' AND DAY 'Monday' TIME '17:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '18:00:00' AND DAY 'Monday' TIME '18:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '19:00:00' AND DAY 'Monday' TIME '19:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Monday' TIME '20:00:00' AND DAY 'Monday' TIME '20:30:00';

ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '01:00:00' AND DAY 'Tuesday' TIME '01:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '02:00:00' AND DAY 'Tuesday' TIME '02:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '03:00:00' AND DAY 'Tuesday' TIME '03:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '04:00:00' AND DAY 'Tuesday' TIME '04:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '05:00:00' AND DAY 'Tuesday' TIME '05:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '06:00:00' AND DAY 'Tuesday' TIME '06:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '07:00:00' AND DAY 'Tuesday' TIME '07:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '08:00:00' AND DAY 'Tuesday' TIME '08:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '09:00:00' AND DAY 'Tuesday' TIME '09:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '10:00:00' AND DAY 'Tuesday' TIME '10:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '11:00:00' AND DAY 'Tuesday' TIME '11:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '12:00:00' AND DAY 'Tuesday' TIME '12:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '13:00:00' AND DAY 'Tuesday' TIME '13:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '14:00:00' AND DAY 'Tuesday' TIME '14:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '15:00:00' AND DAY 'Tuesday' TIME '15:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '16:00:00' AND DAY 'Tuesday' TIME '16:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '17:00:00' AND DAY 'Tuesday' TIME '17:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '18:00:00' AND DAY 'Tuesday' TIME '18:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '19:00:00' AND DAY 'Tuesday' TIME '19:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Tuesday' TIME '20:00:00' AND DAY 'Tuesday' TIME '20:30:00';

ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '01:00:00' AND DAY 'Wednesday' TIME '01:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '02:00:00' AND DAY 'Wednesday' TIME '02:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '03:00:00' AND DAY 'Wednesday' TIME '03:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '04:00:00' AND DAY 'Wednesday' TIME '04:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '05:00:00' AND DAY 'Wednesday' TIME '05:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '06:00:00' AND DAY 'Wednesday' TIME '06:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '07:00:00' AND DAY 'Wednesday' TIME '07:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '08:00:00' AND DAY 'Wednesday' TIME '08:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '09:00:00' AND DAY 'Wednesday' TIME '09:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '10:00:00' AND DAY 'Wednesday' TIME '10:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '11:00:00' AND DAY 'Wednesday' TIME '11:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '12:00:00' AND DAY 'Wednesday' TIME '12:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '13:00:00' AND DAY 'Wednesday' TIME '13:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '14:00:00' AND DAY 'Wednesday' TIME '14:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '15:00:00' AND DAY 'Wednesday' TIME '15:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '16:00:00' AND DAY 'Wednesday' TIME '16:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '17:00:00' AND DAY 'Wednesday' TIME '17:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '18:00:00' AND DAY 'Wednesday' TIME '18:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '19:00:00' AND DAY 'Wednesday' TIME '19:30:00';
ALTER USER testuser DENY BETWEEN DAY 'Wednesday' TIME '20:00:00' AND DAY 'Wednesday' TIME '20:30:00';

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'testuser'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

ALTER USER testuser DROP DENY FOR DAY 'Tuesday' TIME '01:30:00';

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'testuser'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

ALTER USER testuser DROP DENY FOR DAY 'Wednesday';

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'testuser'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

DROP USER IF EXISTS testuser;

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'testuser'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;
