--
-- Tests for functions that return set PYTHONPATH guc only for gpdb6 plpython3
--
LOAD 'plpython3';
show plpython3.python_path;

set plpython3.python_path='/foo';
show plpython3.python_path;

-- Set up test path functions first.
CREATE FUNCTION test_path_added() 
RETURNS text AS $$
  import sys
  return str(sys.path[0])
$$ language plpython3u;

SELECT test_path_added();

-- when the plpython init can not set again --
set plpython3.python_path='/bar';

-- run again the path will not change still /foo --
SELECT test_path_added();
