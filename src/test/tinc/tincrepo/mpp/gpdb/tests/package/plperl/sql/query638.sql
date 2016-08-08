-- Test: plperl 1
CREATE OR REPLACE FUNCTION perl_int(int) RETURNS INTEGER AS $$
							return undef;
							$$ LANGUAGE plperl;
							  
SELECT perl_int(11);
							  

