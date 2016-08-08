-- Test: plperl 54
CREATE OR REPLACE FUNCTION  array_of_text() RETURNS TEXT [] [] AS $$
						      return 
							  [ 
								  [ 'a"b',undef,'c,d' ], 
								  [ 'e\\f',undef,'g' ] 
							  ];
							  $$ LANGUAGE plperl;
							  
SELECT array_of_text();
							  

