# Test validating concurrent insert to AO with appendonlywriter hashtable entry eviction.
#
# Scenario test validates is concurrent insert to AO
#    - after it has acquired the snapshot and
#    - hashtable entry recording concurrent insert transactions id in
# latestWriteXid got evicted out
#     - and then it performs the segment-file selection for inserting data
# Using serializable transaction here just for easy/comfort to grab snapshot
# before AO segment file selection logic kicks-in. Same test can be easily
# written without serializable by using fault-injector, by suspending insert
# statement after acquiring snapshot but before reaching segment file selection
# logic for AO.

setup
{
    CREATE TABLE appendonly_eof (a int) WITH (appendonly=true);
    INSERT INTO appendonly_eof SELECT * FROM generate_series(1, 100);
}

session "s1"
step "s1begin"  { BEGIN; }
step "s1setguc" { SET test_AppendOnlyHash_eviction_vs_just_marking_not_inuse=1; }
step "s1insert"	{ INSERT INTO appendonly_eof SELECT * FROM generate_series(1, 1000); }
step "s1commit"	{ COMMIT; }

session "s2"
step "s2begin"   { BEGIN ISOLATION LEVEL SERIALIZABLE; }
step "s2select"  { SELECT count(*) from appendonly_eof; }
step "s2insert"	 { INSERT INTO appendonly_eof SELECT * FROM generate_series(1, 10); }
step "s2commit"  { COMMIT; }


permutation "s1begin" "s1setguc" "s1insert" "s2begin" "s2select" "s1commit" "s2insert" "s2commit" "s2select"
