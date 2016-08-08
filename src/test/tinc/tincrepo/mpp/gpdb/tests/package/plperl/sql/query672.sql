-- Test: plperl 35
CREATE OR REPLACE FUNCTION
								perl_out_params_set(out f1 integer, out f2 text, out f3 text)
								RETURNS SETOF record AS $$
								return [
								{ f1 => 1, f2 => 'Hello', f3 =>  'World' },
								{ f1 => 2, f2 => 'Hello', f3 =>  'PostgreSQL' },
								{ f1 => 3, f2 => 'Hello', f3 =>  'PL/Perl' }
								];
								$$ LANGUAGE plperl;
							  
SELECT perl_out_params_set();
							  

