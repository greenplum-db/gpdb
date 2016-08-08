-- @product_version gpdb: [4.3.2-4.3.98]
--
-- DISTRIBUTED TRANSACTIONS
--
--SET debug_print_full_dtm=true;

--
-- start_matchsubs
--
-- # create a match/subs expression
--
-- m/(ERROR|WARNING|CONTEXT|NOTICE):.*The previous session was reset because its gang was disconnected/
-- s/session id \=\s*\d+/session id \= DUMMY/gm
--
-- end_matchsubs
--

--
-- We want to have an error between the point where all segments are prepared and our decision 
-- to write the Distributed Commit record.
--
CREATE TABLE distxact1_1 (a int);
BEGIN;
INSERT INTO distxact1_1 VALUES (1);
INSERT INTO distxact1_1 VALUES (2);
INSERT INTO distxact1_1 VALUES (3);
INSERT INTO distxact1_1 VALUES (4);
INSERT INTO distxact1_1 VALUES (5);
INSERT INTO distxact1_1 VALUES (6);
INSERT INTO distxact1_1 VALUES (7);
INSERT INTO distxact1_1 VALUES (8);
SET debug_abort_after_distributed_prepared = true;
COMMIT;
RESET debug_abort_after_distributed_prepared;

SELECT * FROM distxact1_1;
DROP TABLE distxact1_1;

--
-- We want to have an error during the prepare which will cause a Abort-Some-Prepared broadcast 
-- to cleanup.
--
CREATE TABLE distxact1_2 (a int);
BEGIN;
INSERT INTO distxact1_2 VALUES (21);
INSERT INTO distxact1_2 VALUES (22);
INSERT INTO distxact1_2 VALUES (23);
INSERT INTO distxact1_2 VALUES (24);
INSERT INTO distxact1_2 VALUES (25);
INSERT INTO distxact1_2 VALUES (26);
INSERT INTO distxact1_2 VALUES (27);
INSERT INTO distxact1_2 VALUES (28);
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "protocol";
SET debug_dtm_action_protocol = "prepare";
COMMIT;
RESET debug_dtm_action;
RESET debug_dtm_action_target;
RESET debug_dtm_action_protocol;

SELECT * FROM distxact1_2;
DROP TABLE distxact1_2;


--
-- We want to have an error during the commit-prepared broadcast which will cause a
-- Retry-Commit-Prepared broadcast to cleanup.
--
CREATE TABLE distxact1_3 (a int);
BEGIN;
INSERT INTO distxact1_3 VALUES (31);
INSERT INTO distxact1_3 VALUES (32);
INSERT INTO distxact1_3 VALUES (33);
INSERT INTO distxact1_3 VALUES (34);
INSERT INTO distxact1_3 VALUES (35);
INSERT INTO distxact1_3 VALUES (36);
INSERT INTO distxact1_3 VALUES (37);
INSERT INTO distxact1_3 VALUES (38);
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "protocol";
SET debug_dtm_action_protocol = "commit_prepared";
COMMIT;
SELECT * FROM distxact1_3;
DROP TABLE distxact1_3;
RESET debug_dtm_action;
RESET debug_dtm_action_target;
RESET debug_dtm_action_protocol;

--
-- VARIANT of we want to have an error between the point where all segments are prepared and our decision 
-- to write the Distributed Commit record.  Cause problem during abort-prepared broadcast.  
--
CREATE TABLE distxact1_4 (a int);
BEGIN;
INSERT INTO distxact1_4 VALUES (41);
INSERT INTO distxact1_4 VALUES (42);
INSERT INTO distxact1_4 VALUES (43);
INSERT INTO distxact1_4 VALUES (44);
INSERT INTO distxact1_4 VALUES (45);
INSERT INTO distxact1_4 VALUES (46);
INSERT INTO distxact1_4 VALUES (47);
INSERT INTO distxact1_4 VALUES (48);
SET debug_abort_after_distributed_prepared = true;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "protocol";
SET debug_dtm_action_protocol = "abort_prepared";
COMMIT;
SELECT * FROM distxact1_4;
DROP TABLE distxact1_4;
RESET debug_abort_after_distributed_prepared;
RESET debug_dtm_action;
RESET debug_dtm_action_target;
RESET debug_dtm_action_protocol;

--
-- Fail general commands
--


--
-- Invoke a failure during a CREATE TABLE command.  
--
--SET debug_print_full_dtm=true;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "sql";
SET debug_dtm_action_sql_command_tag = "MPPEXEC UTILITY";
CREATE TABLE distxact2_1 (a int);
RESET debug_dtm_action_sql_command_tag;
RESET debug_dtm_action;
RESET debug_dtm_action_target;

SELECT * FROM distxact2_1;

-- Should succeed
CREATE TABLE distxact2_1 (a int);
DROP TABLE distxact2_1;


--
-- Invoke a failure during a CREATE TABLE command.  
-- Action_Target = 2 is SQL.
--

SET debug_dtm_action = "fail_end_command";
SET debug_dtm_action_target = "sql";
SET debug_dtm_action_sql_command_tag = "MPPEXEC UTILITY";
CREATE TABLE distxact2_2 (a int);
RESET debug_dtm_action_sql_command_tag;
RESET debug_dtm_action;
RESET debug_dtm_action_target;

SELECT * FROM distxact2_2;

-- Should succeed
CREATE TABLE distxact2_2 (a int);
DROP TABLE distxact2_2;


--
-- xact.c DTM related dispatches
--

--
-- Invoke a failure during a SAVEPOINT command.  
--
--SET debug_print_full_dtm=true;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "sql";
SET debug_dtm_action_sql_command_tag = "SAVEPOINT";
BEGIN;
CREATE TABLE distxact3_1 (a int);
SAVEPOINT s;
ROLLBACK;
RESET debug_dtm_action_sql_command_tag;
RESET debug_dtm_action;
RESET debug_dtm_action_target;

SELECT * FROM distxact3_1;

-- Should succeed
CREATE TABLE distxact3_1 (a int);
DROP TABLE distxact3_1;


--
-- Invoke a failure during a RELEASE SAVEPOINT command.  
--
--SET debug_print_full_dtm=true;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "sql";
SET debug_dtm_action_sql_command_tag = "RELEASE";
BEGIN;
CREATE TABLE distxact3_2 (a int);
SAVEPOINT s;
INSERT INTO distxact3_2 VALUES (21);
INSERT INTO distxact3_2 VALUES (22);
INSERT INTO distxact3_2 VALUES (23);
INSERT INTO distxact3_2 VALUES (24);
INSERT INTO distxact3_2 VALUES (25);
INSERT INTO distxact3_2 VALUES (26);
INSERT INTO distxact3_2 VALUES (27);
INSERT INTO distxact3_2 VALUES (28);
RELEASE SAVEPOINT s;
ROLLBACK;
RESET debug_dtm_action_sql_command_tag;
RESET debug_dtm_action;
RESET debug_dtm_action_target;

SELECT * FROM distxact3_2;

-- Should succeed
CREATE TABLE distxact3_2 (a int);
DROP TABLE distxact3_2;



--
-- Invoke a failure during a ROLLBACK TO SAVEPOINT command.  
--
--SET debug_print_full_dtm=true;
SET debug_dtm_action = "fail_begin_command";
SET debug_dtm_action_target = "sql";
SET debug_dtm_action_sql_command_tag = "ROLLBACK";
BEGIN;
CREATE TABLE distxact3_3 (a int);
SAVEPOINT s;
INSERT INTO distxact3_3 VALUES (31);
INSERT INTO distxact3_3 VALUES (32);
INSERT INTO distxact3_3 VALUES (33);
INSERT INTO distxact3_3 VALUES (34);
INSERT INTO distxact3_3 VALUES (35);
INSERT INTO distxact3_3 VALUES (36);
INSERT INTO distxact3_3 VALUES (37);
INSERT INTO distxact3_3 VALUES (38);
ROLLBACK TO SAVEPOINT s;
ROLLBACK;
RESET debug_dtm_action_sql_command_tag;
RESET debug_dtm_action;
RESET debug_dtm_action_target;

SELECT * FROM distxact3_3;

-- Should succeed
CREATE TABLE distxact3_3 (a int);
DROP TABLE distxact3_3;

RESET debug_print_full_dtm;


-- Test cursor/serializable interaction.
-- MPP-3227: pg_dump does this exact sequence.
-- for each table in a database.

drop table if exists dtmcurse_foo;
drop table if exists dtmcurse_bar;

create table dtmcurse_foo (a int, b int);
insert into dtmcurse_foo values (1,2);
insert into dtmcurse_foo values (2,2);

create table dtmcurse_bar as select * from dtmcurse_foo distributed by (b);

begin;
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
DECLARE cursor1 CURSOR FOR SELECT * FROM ONLY dtmcurse_foo order by a;
fetch 1 from cursor1;
close cursor1;
-- MPP-3227: second declare would hang waiting for snapshot,
-- should work just like the first.
DECLARE cursor1 CURSOR FOR SELECT * FROM ONLY dtmcurse_bar order by a;
fetch 1 from cursor1;
close cursor1;
abort;

-- MPP-4504: cursor + InitPlan
begin;
declare c1 cursor for select * from dtmcurse_foo where a = (select min(a) from dtmcurse_foo);
fetch 1 from c1;
close c1;
end;

drop table if exists dtmcurse_foo;
drop table if exists dtmcurse_bar;
