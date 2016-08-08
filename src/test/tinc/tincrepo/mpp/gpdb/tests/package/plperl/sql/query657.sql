-- Test: plperl 20
CREATE OR REPLACE FUNCTION perl_record() RETURNS record AS $$
								return undef;
								$$ LANGUAGE plperl;
							  
SELECT perl_record();
							  

