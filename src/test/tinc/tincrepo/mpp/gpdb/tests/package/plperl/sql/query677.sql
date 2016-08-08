-- Test: plperl 40
CREATE OR REPLACE FUNCTION foo_bad() RETURNS footype AS $$
								return 42;
								$$ LANGUAGE plperl;
							  
SELECT * FROM foo_bad();
							  

