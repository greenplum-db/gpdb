-- Test: plperl 3
CREATE OR REPLACE FUNCTION perl_int(int) RETURNS INTEGER AS $$
							return $_[0] + 1;
							$$ LANGUAGE plperl;
							  
SELECT perl_int(11);
							  

