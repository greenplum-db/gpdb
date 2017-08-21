CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

create table fts_walrep (a int) distributed by (a);

-- suspend before commit record is sent to mirror to simulate mirror going down at this point
select gp_inject_fault('twophase_transaction_commit_prepared', 'suspend', 2);

-- suspend transition request to check that transactions are blocked
select gp_inject_fault('segment_transition_request', 'suspend', 2);

-- test 1: in-flight write transaction is blocked during transition to changetracking
1&: insert into fts_walrep select i from generate_series(1,10)i;

-- no segment should be in changetracking
2: select count(*) from gp_segment_configuration where mode = 'c';

-- kill a mirror to put a primary into changetracking
select kill_postmaster((select fselocation from gp_segment_configuration c, pg_filespace_entry f
                        where c.role='m' and c.content=0 and c.dbid = f.fsedbid));

-- resume commits but they should hang on transition request
select gp_inject_fault('twophase_transaction_commit_prepared', 'reset', 2);

-- wait for gp_segment_configuration to show a primary in changetracking
2: select wait_for_changetracking(120, 1);

-- test 2: new write transaction is blocked during transition to changetracking
2&: insert into fts_walrep select i from generate_series(1,10)i;

-- show that gp_segment_configuration is updated and resume transition request
3: select count(*) from gp_segment_configuration where mode = 'c' and role = 'p' and preferred_role = 'p';
select gp_inject_fault('segment_transition_request', 'reset', 2);

-- resume sessions for test 1 and test 2
1<:
2<:

-- show that a primary is still in changetracking
3: select count(*) from gp_segment_configuration where mode = 'c' and role = 'p' and preferred_role = 'p';
