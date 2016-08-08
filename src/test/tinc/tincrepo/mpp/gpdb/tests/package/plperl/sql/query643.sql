-- Test: plperl 6
CREATE OR REPLACE FUNCTION perl_set_int(int) RETURNS SETOF INTEGER AS $$
							return undef;
							$$ LANGUAGE plperl;
							  
SELECT perl_set_int(5);
							  

