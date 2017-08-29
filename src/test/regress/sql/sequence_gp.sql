--
-- Test Overflow with NO CYCLE
--
CREATE TABLE tmp_table (a int);
INSERT INTO tmp_table VALUES (0),(1),(2),(3);

-- Test execution of nextval on master with CACHE 1
CREATE SEQUENCE tmp_seq INCREMENT 1 MINVALUE 1 MAXVALUE 2 START 1 CACHE 1 NO CYCLE;
SELECT nextval('tmp_seq');
SELECT nextval('tmp_seq');
-- Fails because it reaches MAXVALUE
SELECT nextval('tmp_seq');
DROP SEQUENCE tmp_seq;

-- Test that ORCA and Planner return the same results although they produce different execution plans.
CREATE SEQUENCE tmp_seq INCREMENT 1 MINVALUE 1 MAXVALUE 4 START 1 CACHE 1 NO CYCLE;
SELECT val from (SELECT nextval('tmp_seq'), a as val FROM tmp_table ORDER BY a) as val ORDER BY val;
DROP SEQUENCE tmp_seq;

-- Test execution of nextval on master with CACHE > 1
CREATE SEQUENCE tmp_seq INCREMENT 1 MINVALUE 1 MAXVALUE 2 START 1 CACHE 20 NO CYCLE;
SELECT nextval('tmp_seq');
SELECT nextval('tmp_seq');
-- Fails because it reaches MAXVALUE
SELECT nextval('tmp_seq');
DROP SEQUENCE tmp_seq;

-- Test execution of nextval on master (when optimizer = on) and segments (when optimizer=off) with CACHE > 1
CREATE SEQUENCE tmp_seq INCREMENT 1 MINVALUE 1 MAXVALUE 4 START 1 CACHE 20 NO CYCLE;
SELECT nextval('tmp_seq'), a FROM tmp_table ORDER BY a;
DROP SEQUENCE tmp_seq;

DROP TABLE tmp_table; 
