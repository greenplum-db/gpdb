CREATE TABLE tbl_func(i int);

CREATE FUNCTION add(integer,integer) RETURNS integer AS 'select $1 + $2;'
LANGUAGE SQL IMMUTABLE
RETURNS NULL ON NULL INPUT;
