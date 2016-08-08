-- Test: plperl 28
CREATE OR REPLACE FUNCTION perl_record_set() RETURNS SETOF record AS $$
								return [
								{ f1 => 1, f2 => 'Hello', f3 =>  'World' },
								undef,
								{ f1 => 3, f2 => 'Hello', f3 =>  'PL/Perl' }
								];
								$$  LANGUAGE plperl;
							  
SELECT perl_record_set();
							  

