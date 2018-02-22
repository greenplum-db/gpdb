-- Tests exception handling of GPDB PL/PgSQL UDF
-- It exercises:
--  1. PROTOCOL or SQL type of dtm_action_target
--  2. Various levels of sub-transactions
--  3. dtm_action_protocol(PROTOCOL): subtransaction_begin, subtransaction_rollback or subtransaction_release
--  4. dtm_action: fail_begin_command, fail_end_command or panic_begin_comand
--
-- debug_dtm_action: Using this can specify what action to be
-- triggered/simulated and at what point like error / panic / delay
-- and at start or end command after receiving by the segment.

-- debug_dtm_action_segment: Using this can specify segment number to
-- trigger the specified dtm_action.

-- debug_dtm_action_target: Allows to set target for specified
-- dtm_action should it be DTM protocol command or SQL command from
-- master to segment.

-- debug_dtm_action_protocol: Allows to specify sub-type of DTM
-- protocol for which to perform specified dtm_action (like prepare,
-- abort_no_prepared, commit_prepared, abort_prepared,
-- subtransaction_begin, subtransaction_release,
-- subtransaction_rollback, etc...
--
-- debug_dtm_action_sql_command_tag: If debug_dtm_action_target is sql
-- then this parameter can be used to set the type of sql that should
-- trigger the exeception. Ex: 'MPPEXEC UPDATE'

-- debug_dtm_action_nestinglevel: This allows to optional specify at
-- which specific depth level in transaction to take the specified
-- dtm_action. This apples only to target with protocol and not SQL.
--
--
-- start_matchsubs
-- s/\s+\(.*\.[ch]:\d+\)/ (SOMEFILE:SOMEFUNC)/
-- m/ /
-- m/transaction \d+/
-- s/transaction \d+/transaction /
-- m/transaction -\d+/
-- s/transaction -\d+/transaction/
-- end_matchsubs
CREATE OR REPLACE FUNCTION test_excep (arg INTEGER) RETURNS INTEGER
AS $$
    DECLARE res INTEGER;
    BEGIN
        res := 100 / arg;
        RETURN res;
    EXCEPTION
        WHEN division_by_zero
        THEN  RETURN 999;
    END;
$$
LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION test_protocol_allseg(mid int, mshop int, mgender character) RETURNS VOID AS
$$
DECLARE tfactor int default 0;
BEGIN
  BEGIN
  CREATE TABLE employees(id int, shop_id int, gender character) DISTRIBUTED BY (id);
  
  INSERT INTO employees VALUES (0, 1, 'm');
  END;
 BEGIN
  BEGIN
    IF EXISTS (select 1 from employees where id = mid) THEN
        RAISE EXCEPTION 'Duplicate employee id';
    ELSE
         IF NOT (mshop between 1 AND 2) THEN
            RAISE EXCEPTION 'Invalid shop id' ;
        END IF;
    END IF;
    SELECT * INTO tfactor FROM test_excep(0);
    BEGIN
        INSERT INTO employees VALUES (mid, mshop, mgender);
    EXCEPTION
            WHEN OTHERS THEN
            BEGIN
                RAISE NOTICE 'catching the exception ...3';
            END;
    END;
   EXCEPTION
       WHEN OTHERS THEN
          RAISE NOTICE 'catching the exception ...2';
   END;
 EXCEPTION
     WHEN OTHERS THEN
          RAISE NOTICE 'catching the exception ...1';
 END;
END;
$$
LANGUAGE plpgsql;

SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_begin;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=0;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
select * from employees;
--
--
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_release;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=0;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
select * from employees;
--
--
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_release;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=4;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
select * from employees;
--
--
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_rollback;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=3;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
select * from employees;
--
--
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_rollback;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=0;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
select * from employees;
--
--
SET debug_dtm_action_segment=1;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_begin;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=3;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
select * from employees;
