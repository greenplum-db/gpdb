-- Create a function that will be added as an operator
CREATE FUNCTION my_pk_schema.is_zero(int) RETURNS TABLE(f1 boolean)
AS $$ select $1 = 0 $$  LANGUAGE SQL;

CREATE OPERATOR my_pk_schema.!# (PROCEDURE = my_pk_schema.is_zero,LEFTARG = integer);
