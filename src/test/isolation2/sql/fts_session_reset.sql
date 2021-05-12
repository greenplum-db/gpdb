-- This test performs segment reconfiguration when a distributed 
-- transaction is in progress. The expectation is that the first 
-- command in the transaction after reconfiguration should fail. It 
-- verifies a bug where a stale gang was reused in such a case, if the 
-- failed primary happened to be up and listening.

-- start_ignore
! gpconfig -c gp_fts_probe_timeout -v 2;
! gpconfig -c gp_fts_probe_interval -v '10s';
! gpstop -rai;
-- end_ignore

create table test_fts_session_reset(c1 int);
create table test_fts_session_reset2(c1 int);
insert into test_fts_session_reset2 values(1);
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

-- Helper function
CREATE or REPLACE FUNCTION wait_until_segments_are_down(num_segs int)
RETURNS bool AS
$$
declare
retries int; /* in func */
begin /* in func */
  retries := 1200; /* in func */
  loop /* in func */
    if (select count(*) = num_segs from gp_segment_configuration where status = 'd') then /* in func */
      return true; /* in func */
    end if; /* in func */
    if retries <= 0 then /* in func */
      return false; /* in func */
    end if; /* in func */
    perform pg_sleep(0.1); /* in func */
    retries := retries - 1; /* in func */
  end loop; /* in func */
end; /* in func */
$$ language plpgsql;

create or replace function wait_until_all_segments_synchronized() returns text as $$
begin
	for i in 1..1200 loop
		if (select count(*) = 0 from gp_segment_configuration where content != -1 and mode != 's') then
			return 'OK'; /* in func */
		end if; /* in func */
		perform pg_sleep(0.1); /* in func */
	end loop; /* in func */
	return 'Fail'; /* in func */
end; /* in func */
$$ language plpgsql;

1: BEGIN;
-- inject a fault in funcion cdbdisp_finish_command on coordinator, to suspend the execution of insert statement.
1: select gp_inject_fault_new('cdbdisp_finish_command', 'suspend', dbid) from gp_segment_configuration where content=-1 and role='p';
-- insert data, it will hang here
1&: insert into test_fts_session_reset select * from generate_series(1,20);

-- inject a fault in primary segment, that will cause the coordinator to mark this primary down.
3: select gp_inject_fault_new('segment_probe_response', 'sleep', '', '', '', 1, -1, 600, dbid) from gp_segment_configuration where content=0 and preferred_role='p';
-- wait one primary become down.
2: select wait_until_segments_are_down(1);
-- reset the fault in session1 to make session1 continue
2: select gp_inject_fault_new('cdbdisp_finish_command', 'reset', dbid) from gp_segment_configuration where content=-1 and role='p';

-- insert data again and commit in serssion1, expect error and abort transaction
1<:
1: truncate test_fts_session_reset2;
1: commit;

1q:
2q:

-- start_ignore
! gprecoverseg -a;
! gprecoverseg -ar;
-- end_ignore
select wait_until_all_segments_synchronized();
-- start_ignore
! gpconfig -r gp_fts_probe_timeout;
! gpconfig -r gp_fts_probe_interval;
! gpstop -rai;
-- end_ignore
