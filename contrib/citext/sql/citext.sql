--
--  Test citext datatype
--

CREATE EXTENSION citext;

-- Test the operators and indexing functions

-- Test = and <>.
SELECT 'a'::citext = 'a'::citext AS t;
SELECT 'a'::citext = 'A'::citext AS t;
SELECT 'a'::citext = 'A'::text AS f;        -- text wins the discussion
SELECT 'a'::citext = 'b'::citext AS f;
SELECT 'a'::citext = 'ab'::citext AS f;
SELECT 'a'::citext <> 'ab'::citext AS t;

-- Test > and >=
SELECT 'B'::citext > 'a'::citext AS t;
SELECT 'b'::citext >  'A'::citext AS t;
SELECT 'B'::citext >  'b'::citext AS f;
SELECT 'B'::citext >= 'b'::citext AS t;

-- Test < and <=
SELECT 'a'::citext <  'B'::citext AS t;
SELECT 'a'::citext <= 'B'::citext AS t;

-- Test implicit casting. citext casts to text, but not vice-versa.
SELECT 'a'::citext = 'a'::text   AS t;
SELECT 'A'::text  <> 'a'::citext AS t;

SELECT 'B'::citext <  'a'::text AS t;  -- text wins.
SELECT 'B'::citext <= 'a'::text AS t;  -- text wins.

SELECT 'a'::citext >  'B'::text AS t;  -- text wins.
SELECT 'a'::citext >= 'B'::text AS t;  -- text wins.

-- Test implicit casting. citext casts to varchar, but not vice-versa.
SELECT 'a'::citext = 'a'::varchar   AS t;
SELECT 'A'::varchar  <> 'a'::citext AS t;

SELECT 'B'::citext <  'a'::varchar AS t;  -- varchar wins.
SELECT 'B'::citext <= 'a'::varchar AS t;  -- varchar wins.

SELECT 'a'::citext >  'B'::varchar AS t;  -- varchar wins.
SELECT 'a'::citext >= 'B'::varchar AS t;  -- varchar wins.

-- A couple of longer examples to ensure that we don't get any issues with bad
-- conversions to char[] in the c code. Yes, I did do this.

SELECT 'aardvark'::citext = 'aardvark'::citext AS t;
SELECT 'aardvark'::citext = 'aardVark'::citext AS t;

-- Check the citext_cmp() function explicitly.
SELECT citext_cmp('aardvark'::citext, 'aardvark'::citext) AS zero;
SELECT citext_cmp('aardvark'::citext, 'aardVark'::citext) AS zero;
SELECT citext_cmp('AARDVARK'::citext, 'AARDVARK'::citext) AS zero;
SELECT citext_cmp('B'::citext, 'a'::citext) > 0 AS true;

-- Do some tests using a table and index.

CREATE TEMP TABLE try (
   id serial,
   name citext
);

INSERT INTO try (name)
VALUES ('a'), ('ab'), ('â'), ('aba'), ('b'), ('ba'), ('bab'), ('AZ');

SELECT name, 'a' = name AS eq_a   FROM try WHERE name <> 'â';
SELECT name, 'a' = name AS t      FROM try where name = 'a';
SELECT name, 'A' = name AS "eq_A" FROM try WHERE name <> 'â';
SELECT name, 'A' = name AS t      FROM try where name = 'A';
SELECT name, 'A' = name AS t      FROM try where name = 'A';

-- Make sure that citext_smaller() and citext_larger() work properly.
SELECT citext_smaller( 'ab'::citext, 'ac'::citext ) = 'ab' AS t;
SELECT citext_smaller( 'ABC'::citext, 'bbbb'::citext ) = 'ABC' AS t;
SELECT citext_smaller( 'aardvark'::citext, 'Aaba'::citext ) = 'Aaba' AS t;
SELECT citext_smaller( 'aardvark'::citext, 'AARDVARK'::citext ) = 'AARDVARK' AS t;

SELECT citext_larger( 'ab'::citext, 'ac'::citext ) = 'ac' AS t;
SELECT citext_larger( 'ABC'::citext, 'bbbb'::citext ) = 'bbbb' AS t;
SELECT citext_larger( 'aardvark'::citext, 'Aaba'::citext ) = 'aardvark' AS t;

-- Test aggregate functions and sort ordering

CREATE TEMP TABLE srt (
   id serial,
   name CITEXT
);

INSERT INTO srt (name)
VALUES ('abb'),
       ('ABA'),
       ('ABC'),
       ('abd');

-- Check the min() and max() aggregates, with and without index.
set enable_seqscan = off;
SELECT MIN(name) AS "ABA" FROM srt;
SELECT MAX(name) AS abd FROM srt;
reset enable_seqscan;
set enable_indexscan = off;
SELECT MIN(name) AS "ABA" FROM srt;
SELECT MAX(name) AS abd FROM srt;
reset enable_indexscan;

-- Check sorting likewise
set enable_seqscan = off;
SELECT name FROM srt ORDER BY name;
reset enable_seqscan;
set enable_indexscan = off;
SELECT name FROM srt ORDER BY name;
reset enable_indexscan;

-- Test assignment casts.
SELECT LOWER(name) as aba FROM srt WHERE name = 'ABA'::text;
SELECT LOWER(name) as aba FROM srt WHERE name = 'ABA'::varchar;
SELECT LOWER(name) as aba FROM srt WHERE name = 'ABA'::bpchar;
SELECT LOWER(name) as aba FROM srt WHERE name = 'ABA';
SELECT LOWER(name) as aba FROM srt WHERE name = 'ABA'::citext;

-- LIKE should be case-insensitive
SELECT name FROM srt WHERE name     LIKE '%a%' ORDER BY name;
SELECT name FROM srt WHERE name NOT LIKE '%b%' ORDER BY name;
SELECT name FROM srt WHERE name     LIKE '%A%' ORDER BY name;
SELECT name FROM srt WHERE name NOT LIKE '%B%' ORDER BY name;

-- ~~ should be case-insensitive
SELECT name FROM srt WHERE name ~~  '%a%' ORDER BY name;
SELECT name FROM srt WHERE name !~~ '%b%' ORDER BY name;
SELECT name FROM srt WHERE name ~~  '%A%' ORDER BY name;
SELECT name FROM srt WHERE name !~~ '%B%' ORDER BY name;

-- ~ should be case-insensitive
SELECT name FROM srt WHERE name ~  '^a' ORDER BY name;
SELECT name FROM srt WHERE name !~ 'a$' ORDER BY name;
SELECT name FROM srt WHERE name ~  '^A' ORDER BY name;
SELECT name FROM srt WHERE name !~ 'A$' ORDER BY name;

-- SIMILAR TO should be case-insensitive.
SELECT name FROM srt WHERE name SIMILAR TO '%a.*';
SELECT name FROM srt WHERE name SIMILAR TO '%A.*';

-- EXTENSION UPDATE
ALTER EXTENSION citext UPDATE TO '1.1';

-- ALTER EXTENSION SCHMEA
CREATE SCHEMA s1;
ALTER EXTENSION citext SET SCHEMA s1;
\dx
ALTER EXTENSION citext SET SCHEMA public;
SELECT name FROM srt WHERE name = 'ABB';

-- DROP EXTENSION
DROP EXTENSION citext CASCADE;
