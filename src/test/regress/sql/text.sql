--
-- TEXT
--

SELECT text 'this is a text string' = text 'this is a text string' AS true;

SELECT text 'this is a text string' = text 'this is a text strin' AS false;

CREATE TABLE TEXT_TBL (f1 text);

INSERT INTO TEXT_TBL VALUES ('doh!');
INSERT INTO TEXT_TBL VALUES ('hi de ho neighbor');

SELECT '' AS two, * FROM TEXT_TBL order by f1;

--
-- TEXT CASTING TO/FROM ANY TYPE
--
SELECT '1'::bool::text;
SELECT array[1,2]::text;
SELECT '{1,2}'::text::integer[];

CREATE TYPE usr_define_type as (id int, name text);
SELECT '(1,abc)'::text::usr_define_type;
