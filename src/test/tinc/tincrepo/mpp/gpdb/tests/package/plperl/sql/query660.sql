-- Test: plperl 23
CREATE OR REPLACE FUNCTION perl_record() RETURNS record AS $$
								return { f2 => 'hello', f1 => 1, f3 => 'world' };
								$$ LANGUAGE plperl;
							  
SELECT perl_record();
							  

