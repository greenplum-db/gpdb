\echo '-- start_ignore'
DROP TYPE IF EXISTS testrowperl cascade;
\echo '-- end_ignore'

-- Test: plperl 10
CREATE TYPE testrowperl AS (f1 integer, f2 text, f3 text);
							  
CREATE OR REPLACE FUNCTION perl_row() RETURNS testrowperl AS $$
							return undef;
							$$ LANGUAGE plperl;
							  
SELECT perl_row();
							  

