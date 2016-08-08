-- Test: plperl 44
CREATE OR REPLACE FUNCTION foo_set_bad() RETURNS SETOF footype AS $$
								return  [
								[1, 2],
								[3, 4]
								];
								$$ LANGUAGE plperl;
							  
SELECT * FROM foo_set_bad();
							  

