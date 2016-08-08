-- Test: plperl 8
CREATE OR REPLACE FUNCTION perl_set_int(int) RETURNS SETOF INTEGER AS $$
							return [0..$_[0]];
							$$ LANGUAGE plperl;
							  
SELECT perl_set_int(5);
							  

