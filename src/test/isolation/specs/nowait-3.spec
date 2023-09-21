# Test NOWAIT with tuple locks.
# GPDB: have to run sessions that have SELECT ... FOR ... w/ planner because 
# ORCA would upgrade lock to ExclusiveLock.

setup
{
  CREATE TABLE foo (
	id int PRIMARY KEY,
	data text NOT NULL
  );
  INSERT INTO foo VALUES (1, 'x');
}

teardown
{
  DROP TABLE foo;
}

session s1
setup		{ SET optimizer=off; BEGIN; }
step s1a	{ SELECT * FROM foo FOR UPDATE; }
step s1b	{ COMMIT; }

session s2
setup		{ SET optimizer=off; BEGIN; }
step s2a	{ SELECT * FROM foo FOR UPDATE; }
step s2b	{ COMMIT; }

session s3
setup		{ SET optimizer=off; BEGIN; }
step s3a	{ SELECT * FROM foo FOR UPDATE NOWAIT; }
step s3b	{ COMMIT; }

# s3 skips to second record due to tuple lock held by s2
permutation s1a s2a s3a s1b s2b s3b
