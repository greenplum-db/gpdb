-- WARNING: This file is executed against the postgres database. If
-- objects are to be manipulated in other databases, make sure to
-- change to the correct database first.

-- The pg_get_viewdef() function was fixed to properly show views
-- defined with gp_dist_random() but it will diff in this binary swap
-- test suite. Ignore by removing the view.
\c regression;
DROP VIEW IF EXISTS locktest_segments;
DROP VIEW IF EXISTS notdisview;

\c isolation2test;
DROP VIEW IF EXISTS locktest_segments;
