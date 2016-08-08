-- Test: plperl 18
CREATE OR REPLACE FUNCTION perl_set() RETURNS SETOF testrowperl AS $$
								return [
								{ f1 => 1, f2 => 'Hello', f3 =>  'World' },
								{ f1 => 2, f2 => 'Hello', f3 =>  'PostgreSQL' },
								{ f1 => 3, f2 => 'Hello', f3 =>  'PL/Perl' }
								];
								$$  LANGUAGE plperl;
							  
SELECT perl_set();
							  

