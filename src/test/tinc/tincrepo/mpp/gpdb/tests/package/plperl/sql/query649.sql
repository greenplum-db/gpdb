-- Test: plperl 12
CREATE OR REPLACE FUNCTION perl_row() RETURNS testrowperl AS $$
							return { f2 => 'hello', f1 => 1, f3 => 'world' };
							$$ LANGUAGE plperl;
							  
SELECT perl_row();
							  

