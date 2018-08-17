-- By design, every boolean query (which doesn't error out) in this test must return true.
-- For reference, 2011-08-30 was a Tuesday!
--
CREATE FUNCTION check_auth_time_constraints(cstring, timestamptz)
    RETURNS bool
    AS '@abs_builddir@/regress@DLSUFFIX@'
    LANGUAGE C IMMUTABLE STRICT NO SQL;

--
-- invalid timestamp should error out
--
SELECT check_auth_time_constraints('abc', '5');

--
-- basic day level constraints
--
CREATE ROLE abc DENY DAY 2;
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-29 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 12:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-08-31 00:00:00');
DROP ROLE abc;

--
-- basic time level constraints
--
CREATE ROLE abc WITH LOGIN DENY DAY 'Saturday';
DROP ROLE abc;
CREATE ROLE abc DENY DAY 2 TIME '13:15:34';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 13:15:33');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 13:15:34');
SELECT check_auth_time_constraints('abc', '2011-08-30 13:15:35');
DROP ROLE abc;

--
-- some similar CREATE ROLE coverage
--
CREATE ROLE abc DENY BETWEEN DAY 2 AND DAY 3;
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-29 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-09-01 00:00:00');
DROP ROLE abc;

CREATE ROLE abc DENY BETWEEN DAY 2 TIME '05:30:30' AND DAY 3;
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 05:30:29');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 05:30:30');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-09-01 00:00:00');
DROP ROLE abc;

CREATE ROLE abc DENY BETWEEN DAY 2 AND DAY 3 TIME '11:34:53';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-29 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 11:34:53');
SELECT check_auth_time_constraints('abc', '2011-08-31 11:34:54');
DROP ROLE abc;

CREATE ROLE abc DENY BETWEEN DAY 2 TIME '15:24:20' AND DAY 3 TIME '11:34:53';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 15:24:19');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 15:24:20');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 11:34:53');
SELECT check_auth_time_constraints('abc', '2011-08-31 11:34:54');
DROP ROLE abc;

--
-- ALTER ROLE grammar coverage (some repetition here)
--
CREATE ROLE abc;
ALTER ROLE abc DENY DAY 2;
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-29 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 12:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-08-31 00:00:00');
DROP ROLE abc;

CREATE ROLE abc;
ALTER ROLE abc DENY DAY 2 TIME '13:15:34';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 13:15:33');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 13:15:34');
SELECT check_auth_time_constraints('abc', '2011-08-30 13:15:35');
DROP ROLE abc;

CREATE ROLE abc;
ALTER ROLE abc DENY BETWEEN DAY 2 AND DAY 3;
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-29 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-09-01 00:00:00');
DROP ROLE abc;

CREATE ROLE abc;
ALTER ROLE abc DENY BETWEEN DAY 2 TIME '05:30:30' AND DAY 3;
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 05:30:29');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 05:30:30');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-09-01 00:00:00');
DROP ROLE abc;

CREATE ROLE abc;
ALTER ROLE abc DENY BETWEEN DAY 2 AND DAY 3 TIME '11:34:53';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-29 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 11:34:53');
SELECT check_auth_time_constraints('abc', '2011-08-31 11:34:54');
DROP ROLE abc;

CREATE ROLE abc;
ALTER ROLE abc DENY BETWEEN DAY 2 TIME '15:24:20' AND DAY 3 TIME '11:34:53';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 15:24:19');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 15:24:20');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 11:34:53');
SELECT check_auth_time_constraints('abc', '2011-08-31 11:34:54');
DROP ROLE abc;

--
-- very minimal coverage of CREATE USER and ALTER USER
--
CREATE USER abc DENY DAY 2;
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
select NOT check_auth_time_constraints('abc', '2011-08-30 12:00:00');
ALTER USER abc DENY DAY 3 TIME '12:00:00';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
select NOT check_auth_time_constraints('abc', '2011-08-31 12:00:00');
ALTER USER abc DROP DENY FOR DAY 2 TIME '12:00:00';
DROP USER abc;

--
-- CREATE ROLE w/ multiple deny specifications
--
CREATE ROLE abc DENY DAY 0 DENY DAY 2 DENY BETWEEN DAY 4 AND DAY 5 TIME '13:00:00';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-27 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-28 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-28 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-08-29 00:00:00');
SELECT check_auth_time_constraints('abc', '2011-08-29 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-08-31 00:00:00');
SELECT check_auth_time_constraints('abc', '2011-08-31 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-09-01 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-09-02 13:00:00');
SELECT check_auth_time_constraints('abc', '2011-09-02 13:00:01');
DROP ROLE abc;

--
-- ALTER ROLE w/ multiple deny specifications
--
CREATE ROLE abc;
ALTER ROLE abc DENY DAY 0 DENY DAY 2 DENY BETWEEN DAY 4 AND DAY 5 TIME '13:00:00';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-27 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-28 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-28 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-08-29 00:00:00');
SELECT check_auth_time_constraints('abc', '2011-08-29 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-08-31 00:00:00');
SELECT check_auth_time_constraints('abc', '2011-08-31 23:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-09-01 00:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-09-02 13:00:00');
SELECT check_auth_time_constraints('abc', '2011-09-02 13:00:01');
DROP ROLE abc;

--
-- ALTER ROLE DROP
-- This syntax drops any rule that *overlaps* with the specified point/interval.
--
CREATE ROLE abc DENY DAY 2;
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 12:00:00');
ALTER ROLE abc DROP DENY FOR DAY 2 TIME '13:00';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 12:00:00');
DROP ROLE abc;

CREATE ROLE abc;
ALTER ROLE abc DENY BETWEEN DAY 2 TIME '01:00' AND DAY 2 TIME '02:00';
ALTER ROLE abc DENY BETWEEN DAY 2 TIME '23:00' AND DAY 3 TIME '03:00';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 01:30:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 01:00:00');
ALTER ROLE abc DROP DENY FOR DAY 2;
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 01:30:00');
SELECT check_auth_time_constraints('abc', '2011-08-31 01:00:00');
DROP ROLE abc;

CREATE ROLE abc;
ALTER ROLE abc DENY DAY 0 DENY DAY 1;
ALTER ROLE abc DROP DENY FOR DAY 2;
DROP ROLE abc;

--
-- ALTER ROLE w/ multiple deny and drop specifications
-- This should error out, as the expected behavior isn't quite intuitive.
--
CREATE ROLE abc;
ALTER ROLE abc DENY DAY 0 DENY DAY 1 DENY DAY 2 DROP DENY FOR DAY 1;
DROP ROLE abc;

--
-- DROP ROLE must release associated auth. constraints
--
CREATE ROLE abc DENY DAY 0 DENY DAY 1 DENY DAY 3 TIME '3:00 PM';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
DROP ROLE abc;
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';

--
-- Wrap-around intervals
--
CREATE ROLE abc DENY BETWEEN DAY 6 TIME '12:00:00' AND DAY 0;
CREATE ROLE abc;
ALTER ROLE abc DENY BETWEEN DAY 5 AND DAY 1;
DROP ROLE abc;

--
-- English days of week
--
CREATE ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '12:00:00' AND DAY 2 TIME '1:00 PM';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 11:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 12:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 13:00:00');
SELECT check_auth_time_constraints('abc', '2011-08-30 13:00:01');
DROP ROLE abc;

--
-- English days of week (negative)
--
CREATE ROLE abc DENY DAY 'Monkey' TIME '12:00:00';
CREATE ROLE abc WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '00:00' AND DAY 'Monday' TIME '25:00';
CREATE ROLE abc WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '01:65 AM' AND DAY 'Monday' TIME '12:00 PM';
CREATE ROLE abc WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '-01:00' AND DAY 'Monday' TIME '06:00 PM';
CREATE ROLE abc DENY BETWEEN DAY 'Monday' TIME '00:00' AND DAY 'Monday' TIME '25:00';

--
-- What is '24:00:00'? Apparently, '2011-08-30 24:00:00' == '2011-08-31 00:00:00'.
-- But interpreted as a sole time value, '24:00:00' != '00:00:00' and
-- '24:00:00' > '23:59:59'. So, the following three queries should evaluate to true.
-- In other words, there is no way for the current time to ever be '24:00:00'.
--
CREATE ROLE abc DENY DAY 2 TIME '24:00:00';
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 23:59:59');
SELECT check_auth_time_constraints('abc', '2011-08-30 24:00:00');
SELECT check_auth_time_constraints('abc', '2011-08-31 00:00:00');
DROP ROLE abc;


--
-- Inheritance of time constraints (negative)
--
CREATE ROLE abc DENY BETWEEN DAY 0 AND DAY 6;
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 12:00:00');
CREATE ROLE foo IN ROLE abc;
SELECT check_auth_time_constraints('foo', '2011-08-31 12:00:00');
DROP ROLE foo;
DROP ROLE abc;

--
-- Exercise catalog DML and pg_auth_time_constraint trigger
--
SET allow_system_table_mods=true;
CREATE ROLE abc;
insert into pg_auth_time_constraint (select oid, 2, '12:00:00', 3, '12:00:00' from pg_authid where rolname = 'abc');
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 11:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 12:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 12:00:00');
SELECT check_auth_time_constraints('abc', '2011-08-31 12:00:01');

update pg_auth_time_constraint set start_time = '11:00:00', end_time = '13:00:00' where authid = (select oid from pg_authid where rolname = 'abc');
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-30 10:59:59');
SELECT NOT check_auth_time_constraints('abc', '2011-08-30 11:00:00');
SELECT NOT check_auth_time_constraints('abc', '2011-08-31 13:00:00');
SELECT check_auth_time_constraints('abc', '2011-08-31 13:00:01');

delete from pg_auth_time_constraint where authid = (select oid from pg_authid where rolname = 'abc');
select c.start_day, c.start_time, c.end_day, c.end_time from pg_authid a, pg_auth_time_constraint c where c.authid = a.oid and a.rolname = 'abc';
SELECT check_auth_time_constraints('abc', '2011-08-31 00:00:00');
DROP ROLE abc;
RESET allow_system_table_mods;

--
-- Superuser testing across CREATE ROLE, ALTER ROLE, and trigger
--

create role abc with login nosuperuser nocreaterole;
drop role abc;

create role abc superuser deny day 1;

create role abc superuser;
alter role abc deny day 1;
drop role abc;

create role abc;
alter role abc superuser deny day 1;
drop role abc;

create role abc deny day 1;
alter role abc superuser;
SELECT check_auth_time_constraints('abc', '2011-08-29 12:00:00');
drop role abc;

create role abc superuser;
SET allow_system_table_mods=true;
insert into pg_auth_time_constraint (select oid, 1, '00:00:00', 1, '24:00:00' from pg_authid where rolname = 'abc');
SELECT check_auth_time_constraints('abc', '2011-08-29 12:00:00');
DROP ROLE abc;
RESET allow_system_table_mods;

--
-- More DENY tests.
--
CREATE ROLE abc WITH LOGIN DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';;

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'abc'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '01:00:00' AND DAY 'Monday' TIME '01:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '02:00:00' AND DAY 'Monday' TIME '02:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '03:00:00' AND DAY 'Monday' TIME '03:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '04:00:00' AND DAY 'Monday' TIME '04:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '05:00:00' AND DAY 'Monday' TIME '05:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '06:00:00' AND DAY 'Monday' TIME '06:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '07:00:00' AND DAY 'Monday' TIME '07:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '08:00:00' AND DAY 'Monday' TIME '08:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '09:00:00' AND DAY 'Monday' TIME '09:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '10:00:00' AND DAY 'Monday' TIME '10:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '11:00:00' AND DAY 'Monday' TIME '11:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '12:00:00' AND DAY 'Monday' TIME '12:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '13:00:00' AND DAY 'Monday' TIME '13:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '14:00:00' AND DAY 'Monday' TIME '14:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '15:00:00' AND DAY 'Monday' TIME '15:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '16:00:00' AND DAY 'Monday' TIME '16:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '17:00:00' AND DAY 'Monday' TIME '17:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '18:00:00' AND DAY 'Monday' TIME '18:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '19:00:00' AND DAY 'Monday' TIME '19:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Monday' TIME '20:00:00' AND DAY 'Monday' TIME '20:30:00';

ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '01:00:00' AND DAY 'Tuesday' TIME '01:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '02:00:00' AND DAY 'Tuesday' TIME '02:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '03:00:00' AND DAY 'Tuesday' TIME '03:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '04:00:00' AND DAY 'Tuesday' TIME '04:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '05:00:00' AND DAY 'Tuesday' TIME '05:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '06:00:00' AND DAY 'Tuesday' TIME '06:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '07:00:00' AND DAY 'Tuesday' TIME '07:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '08:00:00' AND DAY 'Tuesday' TIME '08:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '09:00:00' AND DAY 'Tuesday' TIME '09:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '10:00:00' AND DAY 'Tuesday' TIME '10:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '11:00:00' AND DAY 'Tuesday' TIME '11:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '12:00:00' AND DAY 'Tuesday' TIME '12:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '13:00:00' AND DAY 'Tuesday' TIME '13:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '14:00:00' AND DAY 'Tuesday' TIME '14:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '15:00:00' AND DAY 'Tuesday' TIME '15:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '16:00:00' AND DAY 'Tuesday' TIME '16:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '17:00:00' AND DAY 'Tuesday' TIME '17:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '18:00:00' AND DAY 'Tuesday' TIME '18:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '19:00:00' AND DAY 'Tuesday' TIME '19:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Tuesday' TIME '20:00:00' AND DAY 'Tuesday' TIME '20:30:00';

ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '01:00:00' AND DAY 'Wednesday' TIME '01:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '02:00:00' AND DAY 'Wednesday' TIME '02:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '03:00:00' AND DAY 'Wednesday' TIME '03:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '04:00:00' AND DAY 'Wednesday' TIME '04:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '05:00:00' AND DAY 'Wednesday' TIME '05:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '06:00:00' AND DAY 'Wednesday' TIME '06:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '07:00:00' AND DAY 'Wednesday' TIME '07:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '08:00:00' AND DAY 'Wednesday' TIME '08:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '09:00:00' AND DAY 'Wednesday' TIME '09:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '10:00:00' AND DAY 'Wednesday' TIME '10:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '11:00:00' AND DAY 'Wednesday' TIME '11:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '12:00:00' AND DAY 'Wednesday' TIME '12:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '13:00:00' AND DAY 'Wednesday' TIME '13:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '14:00:00' AND DAY 'Wednesday' TIME '14:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '15:00:00' AND DAY 'Wednesday' TIME '15:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '16:00:00' AND DAY 'Wednesday' TIME '16:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '17:00:00' AND DAY 'Wednesday' TIME '17:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '18:00:00' AND DAY 'Wednesday' TIME '18:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '19:00:00' AND DAY 'Wednesday' TIME '19:30:00';
ALTER ROLE abc DENY BETWEEN DAY 'Wednesday' TIME '20:00:00' AND DAY 'Wednesday' TIME '20:30:00';

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'abc'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

ALTER ROLE abc DROP DENY FOR DAY 'Tuesday' TIME '01:30:00';

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'abc'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

ALTER ROLE abc DROP DENY FOR DAY 'Wednesday';

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'abc'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;

DROP ROLE IF EXISTS abc;

SELECT r.rolname, d.start_day, d.start_time, d.end_day, d.end_time
FROM pg_auth_time_constraint d join pg_roles r ON (d.authid = r.oid)
WHERE r.rolname = 'abc'
ORDER BY r.rolname, d.start_day, d.start_time, d.end_day, d.end_time;
