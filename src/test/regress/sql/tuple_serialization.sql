-- Check if motion layer correctly serialize & deserialize tuples in particular cases
CREATE TYPE incomplete_type;

CREATE FUNCTION incomplete_type_in(cstring)
   RETURNS incomplete_type
   AS 'textin'
   LANGUAGE internal IMMUTABLE STRICT;
CREATE FUNCTION incomplete_type_out(incomplete_type)
   RETURNS cstring
   AS 'textout'
   LANGUAGE internal IMMUTABLE STRICT;

CREATE TYPE incomplete_type (
   internallength = variable,
   input = incomplete_type_in,
   output = incomplete_type_out,
   alignment = double,
   storage = EXTENDED,
   default = 'zippo'
);

CREATE TABLE table_with_incomplete_type (id int, incomplete incomplete_type);
INSERT INTO table_with_incomplete_type(id, incomplete) VALUES(1, repeat('abcde', 1000000)::incomplete_type);

-- Turn off output temporarily as the output is quite large
\o /dev/null
SELECT * FROM table_with_incomplete_type;
\o

DROP TABLE table_with_incomplete_type;
DROP TYPE incomplete_type CASCADE;
