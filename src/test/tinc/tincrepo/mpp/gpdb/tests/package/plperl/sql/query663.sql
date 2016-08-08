-- Test: plperl 26
CREATE OR REPLACE FUNCTION perl_record_set() RETURNS SETOF record AS $$
								return undef;
								$$  LANGUAGE plperl;
							  
SELECT perl_record_set();
							  

