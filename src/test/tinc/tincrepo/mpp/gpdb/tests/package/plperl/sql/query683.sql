-- Test: plperl 46
CREATE OR REPLACE FUNCTION perl_get_field(footype, text) RETURNS integer AS $$
							  return $_[0]->{$_[1]};
							  $$ LANGUAGE plperl;
							  
SELECT perl_get_field((11,12), 'x');
							  

