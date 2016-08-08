-- Test: plperl 14
CREATE OR REPLACE FUNCTION perl_set() RETURNS SETOF testrowperl AS $$
								return undef;
								$$  LANGUAGE plperl;
							  
SELECT perl_set();
							  

