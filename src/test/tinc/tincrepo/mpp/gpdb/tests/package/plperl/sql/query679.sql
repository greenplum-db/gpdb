-- Test: plperl 42
CREATE OR REPLACE FUNCTION foo_set_bad() RETURNS SETOF footype AS $$
								return 42;
								$$ LANGUAGE plperl;
							  
SELECT * FROM foo_set_bad();
							  

