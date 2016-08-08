-- Test: plperl 45
CREATE OR REPLACE FUNCTION foo_set_bad() RETURNS SETOF footype AS $$
								return  [
								{ y => 3, z => 4 } 
								];
								$$ LANGUAGE plperl;
							  
SELECT * FROM foo_set_bad();
							  

