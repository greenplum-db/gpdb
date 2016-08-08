\echo '-- start_ignore'
DROP TYPE IF EXISTS footype cascade;
\echo '-- end_ignore'

-- Test: plperl 38
CREATE TYPE footype AS (x INTEGER, y INTEGER);
							  
CREATE OR REPLACE FUNCTION foo_good() RETURNS SETOF footype AS $$
								return [
								{x => 1, y => 2},
								{x => 3, y => 4}
								];
								$$ LANGUAGE plperl;
							  
SELECT * FROM foo_good();
							  

