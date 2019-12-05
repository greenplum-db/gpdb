-- The DELETE was not replayed so should show all 10 rows.
SELECT * FROM gpdb_two_phase_commit_before_acquire_share_lock;

-- The DELETE should have been rebroadcasted so should show no rows.
SELECT * FROM gpdb_two_phase_commit_after_acquire_share_lock;

-- The INSERT happened after the restore point was created
-- so this table should be empty.
SELECT * FROM gpdb_two_phase_commit_after_restore_point;

-- The one-phase commit should have gone through before the restore
-- point was created so it should show up.
SELECT * FROM gpdb_one_phase_commit;
