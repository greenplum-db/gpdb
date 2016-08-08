-- Test: plperl 39
CREATE OR REPLACE FUNCTION foo_bad() RETURNS footype AS $$
								return {y => 3, z => 4};
								$$ LANGUAGE plperl;
							  
SELECT * FROM foo_bad();
							  

