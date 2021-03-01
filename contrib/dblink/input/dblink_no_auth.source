-- start_matchsubs
-- m/^DETAIL\:  FATAL\:  no pg_hba.conf entry for host "(.*)", user "dblink_regression_test", database "contrib_regression"(.*)$/
-- s/^DETAIL\:  FATAL\:  no pg_hba.conf entry for host "(.*)", user "dblink_regression_test", database "contrib_regression"(.*)$/^DETAIL\:  FATAL\:  no pg_hba.conf entry for host "(.*)", user "dblink_regression_test", database "contrib_regression"/
-- end_matchsubs
-- ignore NOTICE message
set client_min_messages=warning;
CREATE EXTENSION IF NOT EXISTS dblink;
reset client_min_messages;
CREATE TABLE foo1(f1 int, f2 text, f3 text[], primary key (f1,f2));
INSERT INTO foo1 VALUES (0,'a','{"a0","b0","c0"}');
INSERT INTO foo1 VALUES (1,'b','{"a1","b1","c1"}');
INSERT INTO foo1 VALUES (2,'c','{"a2","b2","c2"}');
INSERT INTO foo1 VALUES (3,'d','{"a3","b3","c3"}');
INSERT INTO foo1 VALUES (4,'e','{"a4","b4","c4"}');
INSERT INTO foo1 VALUES (5,'f','{"a5","b5","c5"}');
INSERT INTO foo1 VALUES (6,'g','{"a6","b6","c6"}');
INSERT INTO foo1 VALUES (7,'h','{"a7","b7","c7"}');
INSERT INTO foo1 VALUES (8,'i','{"a8","b8","c8"}');
INSERT INTO foo1 VALUES (9,'j','{"a9","b9","c9"}');
-- test foreign data wrapper functionality
CREATE SERVER fdtest1 FOREIGN DATA WRAPPER dblink_fdw
  OPTIONS (dbname 'contrib_regression', host 'localhost');
CREATE USER MAPPING FOR public SERVER fdtest1 OPTIONS (user :'USER');

GRANT USAGE ON FOREIGN SERVER fdtest1 TO dblink_regression_test;
GRANT EXECUTE ON FUNCTION dblink_connect_no_auth(text, text) TO dblink_regression_test;

SET SESSION AUTHORIZATION dblink_regression_test;
-- should fail
SELECT dblink_connect('myconn', 'fdtest1');
-- should succeed
SELECT dblink_connect_no_auth('myconn', 'fdtest1');
SELECT * FROM dblink('myconn','SELECT * FROM foo1') AS t(a int, b text, c text[]);

\c - -
REVOKE USAGE ON FOREIGN SERVER fdtest1 FROM dblink_regression_test;
REVOKE EXECUTE ON FUNCTION dblink_connect_no_auth(text, text) FROM dblink_regression_test;
DROP USER MAPPING FOR public SERVER fdtest1;
DROP SERVER fdtest1;

-- Test dblink_connect_no_auth
ALTER ROLE dblink_regression_test PASSWORD 'test';
GRANT ALL ON TABLE foo1 TO dblink_regression_test;
GRANT EXECUTE ON FUNCTION dblink_connect_no_auth(text, text) TO dblink_regression_test;
GRANT EXECUTE ON FUNCTION dblink_connect_u(text, text) TO dblink_regression_test;
SET SESSION AUTHORIZATION dblink_regression_test;
-- should fail
SELECT dblink_connect('myconn', 'dbname=contrib_regression user=dblink_regression_test host=127.0.0.1');
SELECT dblink_connect_u('myconn', 'dbname=contrib_regression user=dblink_regression_test');
-- gp_reject_internal_tcp_connection default is on
SELECT dblink_connect_no_auth('myconn', 'dbname=contrib_regression user=dblink_regression_test host=127.0.0.1');
-- should succeed
SELECT dblink_connect_no_auth('myconn', 'dbname=contrib_regression user=dblink_regression_test');
SELECT * FROM dblink('myconn','SELECT * FROM foo1') AS t(a int, b text, c text[]);
SELECT dblink_disconnect('myconn');

\c - -
ALTER ROLE dblink_regression_test PASSWORD NULL;
REVOKE ALL ON TABLE foo1 FROM dblink_regression_test;
REVOKE EXECUTE ON FUNCTION dblink_connect_no_auth(text, text) FROM dblink_regression_test;
REVOKE EXECUTE ON FUNCTION dblink_connect_u(text, text) FROM dblink_regression_test;
