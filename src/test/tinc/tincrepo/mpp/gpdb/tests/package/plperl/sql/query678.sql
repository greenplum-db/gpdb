-- Test: plperl 41
CREATE OR REPLACE FUNCTION foo_bad() RETURNS footype AS $$
								return  [
								[1, 2],
								[3, 4]
								];
								$$ LANGUAGE plperl;
							  
SELECT * FROM foo_bad();
							  

