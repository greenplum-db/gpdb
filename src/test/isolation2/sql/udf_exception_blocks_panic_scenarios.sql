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
-- coordinator to segment.

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

-- skip FTS probes always
SELECT gp_inject_fault_infinite('fts_probe', 'skip', 1);
SELECT gp_request_fts_probe_scan();
select gp_wait_until_triggered_fault('fts_probe', 1, 1);
CREATE OR REPLACE FUNCTION test_excep (arg INTEGER) RETURNS INTEGER
AS $$
    DECLARE res INTEGER; /* in func */
    BEGIN /* in func */
        res := 100 / arg; /* in func */
        RETURN res; /* in func */
    EXCEPTION /* in func */
        WHEN division_by_zero /* in func */
        THEN  RETURN 999; /* in func */
    END; /* in func */
$$
LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION test_protocol_allseg(mid int, mshop int, mgender character) RETURNS VOID AS
$$
DECLARE tfactor int default 0; /* in func */
BEGIN /* in func */
  BEGIN /* in func */
  CREATE TABLE employees(id int, shop_id int, gender character) DISTRIBUTED BY (id); /* in func */
  
  INSERT INTO employees VALUES (0, 1, 'm'); /* in func */
  END; /* in func */
 BEGIN /* in func */
  BEGIN /* in func */
    IF EXISTS (select 1 from employees where id = mid) THEN /* in func */
        RAISE EXCEPTION 'Duplicate employee id'; /* in func */
    ELSE /* in func */
         IF NOT (mshop between 1 AND 2) THEN /* in func */
            RAISE EXCEPTION 'Invalid shop id' ; /* in func */
        END IF; /* in func */
    END IF; /* in func */
    SELECT * INTO tfactor FROM test_excep(0); /* in func */
    BEGIN /* in func */
        INSERT INTO employees VALUES (mid, mshop, mgender); /* in func */
    EXCEPTION /* in func */
            WHEN OTHERS THEN /* in func */
            BEGIN /* in func */
                RAISE NOTICE 'catching the exception ...3'; /* in func */
            END; /* in func */
    END; /* in func */
   EXCEPTION /* in func */
       WHEN OTHERS THEN /* in func */
          RAISE NOTICE 'catching the exception ...2'; /* in func */
   END; /* in func */
 EXCEPTION /* in func */
     WHEN OTHERS THEN /* in func */
          RAISE NOTICE 'catching the exception ...1'; /* in func */
 END; /* in func */
END; /* in func */
$$
LANGUAGE plpgsql;
SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;
SET debug_dtm_action_segment=0;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_begin;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=0;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
-- make sure segment recovery is complete after panic.
0U: select 1;
0Uq:
select * from employees;
--
--
SET debug_dtm_action_segment=0;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_release;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=0;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
-- make sure segment recovery is complete after panic.
0U: select 1;
0Uq:
select * from employees;
--
--
SET debug_dtm_action_segment=0;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_release;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=4;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
-- make sure segment recovery is complete after panic.
0U: select 1;
0Uq:
select * from employees;
--
--
SET debug_dtm_action_segment=0;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_rollback;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=3;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
-- make sure segment recovery is complete after panic.
0U: select 1;
0Uq:
select * from employees;
--
--
SET debug_dtm_action_segment=0;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_rollback;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=0;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
-- make sure segment recovery is complete after panic.
0U: select 1;
0Uq:
select * from employees;
--
--
SET debug_dtm_action_segment=0;
SET debug_dtm_action_target=protocol;
SET debug_dtm_action_protocol=subtransaction_begin;
SET debug_dtm_action=panic_begin_command;
SET debug_dtm_action_nestinglevel=3;
DROP TABLE IF EXISTS employees;
select test_protocol_allseg(1, 2,'f');
-- make sure segment recovery is complete after panic.
0U: select 1;
0Uq:
select * from employees;

SELECT gp_inject_fault('fts_probe', 'reset', 1);
