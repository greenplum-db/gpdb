CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

DROP TABLE if exists lu_customer;
CREATE TABLE lu_customer (
    customer_id numeric(28,0),
    cust_first_name character varying(50),
    cust_last_name character varying(50),
    cust_birthdate date,
    address character varying(50),
    income_id numeric(28,0),
    email character varying(50),
    cust_city_id numeric(28,0)
);

BEGIN; 
 SET TRANSACTION ISOLATION LEVEL SERIALIZABLE; 
 CREATE TABLE cursor (a int, b int) distributed by (b); 
 INSERT INTO cursor VALUES (1); 
DECLARE c1 NO SCROLL CURSOR FOR SELECT * FROM cursor;
UPDATE cursor SET a = 2; 
 FETCH ALL FROM c1;
 
 COMMIT; 
 DROP TABLE cursor;

begin;
savepoint x;
create table abc (a int) distributed randomly;
insert into abc values (5);
insert into abc values (10);
 --order 1
declare foo  no scroll cursor for select * from abc order by 1;
fetch from foo;
rollback to x;
fetch from foo;
commit;
begin;
create table abc (a int) distributed randomly;
insert into abc values (5);
insert into abc values (10);
insert into abc values (15);
--order 1
 declare foo  no scroll cursor for select * from abc order by 1;
fetch from foo;
savepoint x;
fetch from foo;
rollback to x;
fetch from foo;
abort;

-- Test to validate cursor QE reader is correctly able to perform visibility in
-- subtransaction, even after QE writer has moved ahead and updated the tuple
CREATE TABLE cursor_writer_reader (a int, b int) DISTRIBUTED BY (a);
BEGIN;
INSERT INTO cursor_writer_reader VALUES(1, 666);
select gp_inject_fault('qe_got_snapshot_and_interconnect', 'suspend', 2);
DECLARE cursor_c2 CURSOR FOR SELECT * FROM cursor_writer_reader WHERE b=666 ORDER BY 1;
SAVEPOINT x;
UPDATE cursor_writer_reader SET b=333 WHERE b=666;
-- start_ignore
select gp_inject_fault('qe_got_snapshot_and_interconnect', 'status', 2);
-- end_ignore
select gp_inject_fault('qe_got_snapshot_and_interconnect', 'resume', 2);
FETCH cursor_c2;
SELECT * FROM cursor_writer_reader WHERE b=666 ORDER BY 1;
END;
select gp_inject_fault('qe_got_snapshot_and_interconnect', 'reset', 2);


-- start_ignore
------------------------------------------------------------------------------
-- LAST MODIFIED:
--     2010-04-04 mgilkey
--         To avoid the error message:
--             ERROR:  Cursor-Reader gang not able to provide correct
--             visibility 2/2, writer modified table while cursor was 
--             scanning.
--         which is caused by a deliberate change (see MPP-8622 and MPP-8655), 
--         I modified the table named "y" so that its distribution policy was 
--         no longer "random".
--
--	2010-04-21: Ngoc
-- 		QA-838 or MPP-8622
-- 		Added the following GUC + tables are created without random distribution
-- 		Test case might be still intermittently failed
------------------------------------------------------------------------------
-- end_ignore
--start_ignore
drop table if exists y_schema.y;
drop schema if exists y_schema;
--end_ignore

create schema y_schema;
create table y_schema.y (a int, b int) distributed by (a); 
Begin;
insert into y_schema.y values(10, 666);
insert into y_schema.y values(20, 666);
insert into y_schema.y values(30, 666);
insert into y_schema.y values(40, 666);
update y_schema.y set b =333 where b =666;
--order 1
declare c0 cursor for select * from y_schema.y where b =333 order by 1;
savepoint x;
update y_schema.y set b =666 where b =333;
fetch c0;
fetch c0;
fetch c0;
fetch c0;
--order 1
declare c1 cursor for select * from y_schema.y where b =333 order by 1;
--order 1
declare c2 cursor for select * from y_schema.y where b =666 order by 1;
fetch c2;
fetch c2;
fetch c2;
fetch c2;
savepoint y;
fetch c1;
fetch c1;
rollback to y;
fetch c2;
fetch c2;
rollback to x;
fetch c0;
fetch c0;
commit;
--start_ignore
drop table if exists y_schema.y;
drop schema if exists y_schema;
--end_ignore


--start_ignore
drop table if exists x_schema.y;
drop schema if exists x_schema;
--end_ignore
create schema x_schema;
 create table x_schema.y (a int, b int) distributed randomly;
begin;
declare c1 cursor for select * from x_schema.y where b =666;
savepoint x;
insert into x_schema.y values(10, 666);
insert into x_schema.y values(20, 666);
insert into x_schema.y values(30, 666);
insert into x_schema.y values(40, 666);
update x_schema.y set b =333 where b =666;
fetch c1;
--order 1
declare c2 cursor for select * from x_schema.y where b =666 order by 1;
fetch c2;
--order 1
declare c3 cursor for select * from x_schema.y where b =333 order by 1;
fetch c3;
fetch c3;
fetch c3;
fetch c3;
fetch c3;
commit;
--start_ignore
drop table if exists x_schema.y;
drop schema if exists x_schema;
--end_ignore

-- QA-838 or MPP-8622
-- Added the following GUC + tables are created without random distribution
-- Test case might be still intermittently failed
-- Ngoc

--start_ignore
drop table if exists z_schema.y;
drop schema if exists z_schema;
--end_ignore

create schema z_schema;
 --create table z_schema.y (a int, b int) distributed randomly;
 create table z_schema.y (a int, b int) distributed by (a);
begin;
insert into z_schema.y values(10, 666);
insert into z_schema.y values(20, 666);
insert into z_schema.y values(30, 666);
insert into z_schema.y values(40, 666);
--order 1
declare c1 cursor for select * from z_schema.y where b =666 order by 1;
savepoint x;
update z_schema.y set b =333 where b =666 ;
rollback to x;
fetch c1;
fetch c1;
fetch c1;
fetch c1;
fetch c1;
commit;
--start_ignore
drop table if exists z_schema.y;
drop schema if exists z_schema;
--end_ignore
--start_ignore
DROP TABLE films;
--end_ignore
CREATE TABLE films (
    code        char(5) CONSTRAINT firstkey PRIMARY KEY,
    title       varchar(40) NOT NULL,
    did         integer NOT NULL,
    date_prod   date,
    kind        varchar(10),
    len         interval hour to minute
) distributed by (code);
INSERT INTO films VALUES
    ('UA502', 'Bananas', 105, '1971-07-13', 'Comedy', '82 minutes');
INSERT INTO films (code, title, did, date_prod, kind)
    VALUES ('T_601', 'Yojimbo', 106, '1961-06-16', 'Drama');

INSERT INTO films (code, title, did, date_prod, kind) VALUES
    ('B6717', 'Tampopo', 110, '1985-02-10', 'Comedy'),
    ('HG120', 'The Dinner Game', 140, DEFAULT, 'Comedy');

BEGIN;
--order
DECLARE liahona SCROLL CURSOR FOR SELECT * FROM films order by 1;
FETCH FORWARD 3 FROM liahona;
MOVE liahona; 
FETCH liahona;
CLOSE liahona;
COMMIT;
--start_ignore
DROP TABLE films;
--end_ignore


--start_ignore
DROP TABLE refcur1;
--end_ignore

CREATE FUNCTION reffunc(refcursor) RETURNS refcursor AS '
BEGIN
    OPEN $1 FOR SELECT col FROM refcur1;
    RETURN $1;
END;
' LANGUAGE plpgsql READS SQL DATA;

CREATE TABLE refcur1 (col text) distributed randomly;

INSERT INTO refcur1 VALUES ('123');
BEGIN;
SELECT reffunc('funccursor');
FETCH ALL IN funccursor;
INSERT INTO refcur1 VALUES ('123');
INSERT INTO refcur1 VALUES ('123');
INSERT INTO refcur1 VALUES ('123');
INSERT INTO refcur1 VALUES ('123');
FETCH ALL IN funccursor;

SELECT reffunc('funccursor2');
COMMIT;
SELECT reffunc('funccursor2');
--start_ignore
DROP TABLE refcur1;
--end_ignore

--start_ignore
DROP TABLE table_1;
DROP TABLE table_2;
--end_ignore

CREATE FUNCTION myfunc(refcursor, refcursor) RETURNS SETOF refcursor AS $$
BEGIN
    OPEN $1 FOR SELECT * FROM table_1;
    RETURN NEXT $1;
    OPEN $2 FOR SELECT * FROM table_2;
    RETURN NEXT $2;
END;
$$ LANGUAGE plpgsql READS SQL DATA;
CREATE TABLE table_1 (a1 text, b1 integer) distributed randomly;
INSERT INTO table_1 VALUES ('abcd',10);
CREATE TABLE table_2 (a1 text, b1 integer) distributed randomly;
INSERT INTO table_2 VALUES ('abcde',110);
BEGIN;
--order 1
SELECT * FROM myfunc('a', 'b'); 
--order 1
FETCH ALL FROM a;
--order 1
FETCH ALL FROM b;
COMMIT;
--start_ignore
DROP TABLE table_1;
DROP TABLE table_2;
--end_ignore

--start_ignore
DROP TABLE if exists mpp_1389; 
--end_ignore

CREATE TABLE mpp_1389(num int, letter text) distributed randomly;

insert into mpp_1389 values('1', 'a');
insert into mpp_1389 values('2', 'b');
insert into mpp_1389 values('3', 'c');
insert into mpp_1389 values('4', 'd');
insert into mpp_1389 values('5', 'e');
insert into mpp_1389 values('6', 'f');
insert into mpp_1389 values('7', 'g');

begin;
--order 1
DECLARE f CURSOR WITH HOLD FOR
select * from mpp_1389
ORDER BY num, letter;
commit;
FETCH FROM f;
--start_ignore
DROP TABLE if exists mpp_1389; 
--end_ignore
--start_ignore
DROP INDEX if exists ctest_id_idx;
DROP TABLE if exists ctest;
--end_ignore

CREATE TABLE ctest (
	id int8,
	name varchar
 ) distributed randomly;

INSERT INTO ctest (id, name) SELECT id, 'Test' || id FROM generate_series(1, 1000) AS id;

CREATE INDEX ctest_id_idx ON ctest(id);

\d ctest;

--
-- Return absolute cursor records using sequential scan & index
--

BEGIN;

SET enable_seqscan =on;
--order 1
DECLARE CUR SCROLL CURSOR FOR SELECT * FROM ctest WHERE id >= 990 order by 1;
FETCH ABSOLUTE 1 IN CUR;
FETCH ABSOLUTE 3 IN CUR;
CLOSE CUR;

SET enable_seqscan = off;
--order 1
DECLARE CUR SCROLL CURSOR FOR SELECT * FROM ctest WHERE id >= 990 order by 1; 
FETCH ABSOLUTE 1 IN CUR;
FETCH ABSOLUTE 3 IN CUR;
CLOSE CUR;

COMMIT;


--
-- Rebuild with btree or bitmap
--

DROP INDEX ctest_id_idx;
CREATE INDEX ctest_id_gist_idx ON ctest USING bitmap(id);


--
-- Now try again... and check if results returned are correct using seq scan. 
--

BEGIN;

SET enable_seqscan =on;
--order 1
DECLARE CUR SCROLL CURSOR FOR SELECT * FROM ctest WHERE id >= 990::bigint order by 1;
FETCH ABSOLUTE 1 IN CUR;
FETCH ABSOLUTE 3 IN CUR;
CLOSE CUR;

SET enable_seqscan = off;
--order 1
DECLARE CUR SCROLL CURSOR FOR SELECT * FROM ctest WHERE id >= 990::bigint order by 1;
FETCH ABSOLUTE 1 IN CUR;
FETCH ABSOLUTE 3 IN CUR;
CLOSE CUR;

COMMIT;
--
-- Change of Gp_interconnect_queue_depth after a Cursor will success
BEGIN;

DECLARE CUR SCROLL CURSOR FOR SELECT * FROM ctest WHERE id >= 990::bigint order by 1;
SET gp_interconnect_queue_depth to 20;
FETCH ABSOLUTE 1 IN CUR;

COMMIT;


--
-- Shared snapshot files for cursor should be gone after transaction commits.
--
CREATE EXTERNAL WEB TABLE check_cursor_files(a int)
EXECUTE 'find base | grep pgsql_tmp | grep _sync | wc -l'
FORMAT 'TEXT';

BEGIN;
DECLARE c CURSOR FOR SELECT * FROM ctest ORDER BY id;
FETCH 1 FROM c;

-- There should be a shared snapshot file on every segment.
SELECT DISTINCT a FROM check_cursor_files;

-- holdable cursor should be ok
DECLARE c_hold CURSOR WITH HOLD FOR SELECT * FROM ctest ORDER BY id;
COMMIT;

-- After commit, the shared snapshot files should be deleted.
SELECT DISTINCT a FROM check_cursor_files;

FETCH 1 FROM c_hold;

--start_ignore
DROP INDEX if exists ctest_id_idx;
DROP TABLE if exists ctest;
--end_ignore
