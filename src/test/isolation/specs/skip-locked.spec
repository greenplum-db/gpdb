# Test SKIP LOCKED when regular row locks can't be acquired.
# GPDB: have to run sessions that have SELECT ... FOR ... w/ planner because 
# ORCA would upgrade lock to ExclusiveLock.

setup
{
  CREATE TABLE queue (
	id	int		PRIMARY KEY,
	data			text	NOT NULL,
	status			text	NOT NULL
  );
  INSERT INTO queue VALUES (1, 'foo', 'NEW'), (2, 'bar', 'NEW');
}

teardown
{
  DROP TABLE queue;
}

session s1
setup		{ SET optimizer=off; BEGIN; }
step s1a	{ SELECT * FROM queue ORDER BY id FOR UPDATE SKIP LOCKED LIMIT 1; }
step s1b	{ SELECT * FROM queue ORDER BY id FOR UPDATE SKIP LOCKED LIMIT 1; }
step s1c	{ COMMIT; }

session s2
setup		{ SET optimizer=off; BEGIN; }
step s2a	{ SELECT * FROM queue ORDER BY id FOR UPDATE SKIP LOCKED LIMIT 1; }
step s2b	{ SELECT * FROM queue ORDER BY id FOR UPDATE SKIP LOCKED LIMIT 1; }
step s2c	{ COMMIT; }
