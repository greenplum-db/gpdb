
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema qp_regexp;
set search_path to qp_regexp;
-- end_ignore
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: creat-tbl-for-reg-exp.sql
-- ----------------------------------------------------------------------


drop table if exists phone_book;

create table phone_book
(
lname char varying(25),
fname char varying(25),
state char(2),
phone_num bigint
)
distributed by (lname);

\copy public.phone_book from '@abs_srcdir@/data/phone_book.txt' delimiter as '|'

drop table if exists phone_book_substr;

create table phone_book_substr
(
lname_substr char(3),
lname char varying(25),
fname char varying(25),
state char(2),
phone_num bigint
)
distributed by (lname_substr);
--distributed by (substr(lname,1,2));

insert into
phone_book_substr
(
lname_substr,
lname,
fname,
state,
phone_num
)
select
substr(lname,1,3),
lname,
fname,
state,
phone_num
from phone_book
order by lname
;

analyze phone_book_substr;

RESET ALL;

-- ----------------------------------------------------------------------
-- Test: query01.sql
-- ----------------------------------------------------------------------

-- Regular Expression
SELECT regexp_matches('foobarbequebaz', '(bar)(beque)');

SELECT regexp_matches('foobarbequebazilbarfbonk', '(b[^b]+)(b[^b]+)', 'g');

SELECT regexp_matches('foobarbequebaz', 'barbeque');

SELECT foo FROM regexp_split_to_table('the quick brown fox jumped over the lazy dog', E'\\\s+') AS foo;

SELECT regexp_split_to_array('the quick brown fox jumped over the lazy dog', E'\\s+');

SELECT foo FROM regexp_split_to_table('the quick brown fox', E'\\s*') AS foo;

SELECT '123' ~ E'^\\d{3}';

SELECT 'abc' SIMILAR TO 'abc'; 
SELECT 'abc' SIMILAR TO 'a'; 
SELECT 'abc' SIMILAR TO '%(b|d)%'; 
SELECT 'abc' SIMILAR TO '(b|c)%'; 

SELECT substring('foobar' from '%#"o_b#"%' for '#'); 
SELECT substring('foobar' from '#"o_b#"%' for '#'); 

SELECT substring('foobar' from 'o.b'); 
SELECT substring('foobar' from 'o(.)b');

SELECT regexp_replace('foobarbaz', 'b..', 'X');
                                   
SELECT regexp_replace('foobarbaz', 'b..', 'X', 'g');
                                   
SELECT regexp_replace('foobarbaz', 'b(..)', E'X\\1Y', 'g');
                                   

SELECT SUBSTRING('XY1234Z', 'Y*([0-9]{1,3})'); 
SELECT SUBSTRING('XY1234Z', 'Y*?([0-9]{1,3})');
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: reg-exp-like-qry003.sql
-- ----------------------------------------------------------------------

select count(*)
from phone_book_substr
;

--17388
---------------------------------------------------------
-- NOT LIKE

select count(*)
from phone_book_substr
where lname_substr LIKE 'Aa' ;
--0 Rows.

select count(*) 
from phone_book_substr 
where lname_substr LIKE 'A%' ;
--621

select count(*)
from phone_book_substr
where lname_substr LIKE 'Z%';
--621

select count(*)
from phone_book_substr
where lname_substr LIKE '_a_';
--648

select count(*)
from phone_book_substr
where lname_substr LIKE '_Z_';
--0 Rows.

select count(*)
from phone_book_substr
where lname_substr LIKE 'Abd';
--0 Rows.

select count(*)
from phone_book_substr
;

--17388
---------------------------------------------------------

select lname_substr
from phone_book_substr
where lname_substr LIKE 'Aa' group by 1 order by 1;
--0 Rows.

select lname_substr 
from phone_book_substr 
where lname_substr LIKE 'A%' group by 1 order by 1;

select count(*)
from phone_book_substr
where lname_substr LIKE 'A%';
--621

select lname_substr
from phone_book_substr
where lname_substr LIKE '_a_' group by 1 order by 1;


select lname_substr
from phone_book_substr
where lname_substr LIKE '_Z_' group by 1 order by 1;
--0 Rows.

select lname_substr
from phone_book_substr
where lname_substr LIKE 'Abd' group by 1 order by 1;
--0 Rows.

-- ~~ is equivalent to LIKE

select count(*)
from phone_book_substr
where lname_substr ~~ 'A%';
--621

select count(*)
from phone_book_substr
where lname_substr ~~ '_b_';
--3240


-- ILIKE  case sensitive LIKE

select count(*)
from phone_book_substr
where lname_substr ILIKE 'a%';

-- ~~* is equivalent to ILIKE

select count(*)
from phone_book_substr
where lname_substr ~~* 'a%';
--621

select count(*)
from phone_book_substr
where lname_substr ~~* '_b_';
--3240

-- NOT LIKE

select lname_substr
from phone_book_substr
where lname_substr NOT LIKE 'Z%' group by 1 order by 1;

-- !~~ is equivalent to NOT LIKE

select count(*)
from phone_book_substr
where lname_substr !~~ 'A%';
--16767

select count(*)
from phone_book_substr
where lname_substr !~~ '_A%';
--17388

select count(*)
from phone_book_substr
where lname_substr !~~ '_b%';
--14148

select count(*)
from phone_book_substr
where lname_substr !~~ '_B%';
--17388

select count(*)
from phone_book_substr
where lname_substr !~~ '_b%';
--14148


-- !~~* is equivalent to NOT ILIKE

select count(*)
from phone_book_substr
where lname_substr !~~* 'a%';
--16767

select count(*)
from phone_book_substr
where lname_substr !~~* '_A%';
--16740

select count(*)
from phone_book_substr
where lname_substr !~~* '_b%';
--14148

select count(*)
from phone_book_substr
where lname_substr !~~* '_B%';
--14148

select count(*)
from phone_book_substr
where lname_substr !~~* '_B_';
--14148

select count(*)
from phone_book_substr
where lname_substr !~~* '_b_';
--14148

-- ARRAY   

--select array(select fname 
--		from phone_book_substr 
--		where lname_substr LIKE '%y%');

RESET ALL;

-- ----------------------------------------------------------------------
-- Test: reg-exp-not-ilike-qry004.sql
-- ----------------------------------------------------------------------

--select count(*)
--from phone_book_substr
--;

--17388
---------------------------------------------------------
-- ILIKE  case sensitive NOT ILIKE
-- NOT ILIKE

select count(*)
from phone_book_substr
where lname_substr NOT ILIKE 'a%';
--16767

select count(*) 
from phone_book_substr 
where lname_substr NOT ILIKE 'A%';
--16767

select count(*)
from phone_book_substr
where lname_substr NOT ILIKE 'Aa';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT ILIKE '_a_';
--16740

select count(*)
from phone_book_substr
where lname_substr NOT ILIKE '_Z_';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT ILIKE 'Abd';
--17388

RESET ALL;

-- ----------------------------------------------------------------------
-- Test: reg-exp-not-like-qry005.sql
-- ----------------------------------------------------------------------

select count(*)
from phone_book_substr
;

--17388
---------------------------------------------------------
-- NOT LIKE

select count(*)
from phone_book_substr
where lname_substr NOT LIKE 'Aa';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT LIKE 'A%' ;
--16767

select count(*)
from phone_book_substr
where lname_substr NOT LIKE 'Z%';
--16767

select count(*)
from phone_book_substr
where lname_substr LIKE '_a_';
--16740

select count(*)
from phone_book_substr
where lname_substr NOT LIKE '_Z_';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT LIKE 'Abd';
--17388

RESET ALL;

-- ----------------------------------------------------------------------
-- Test: reg-exp-not-tilde-ilike-qry007.sql
-- ----------------------------------------------------------------------

--select count(*)
--from phone_book_substr
--;
--17388
---------------------------------------------------------
-- !~~* is equivalent to NOT ILIKE

select count(*)
from phone_book_substr
where lname_substr !~~* 'A%';
--16767

select count(*)
from phone_book_substr
where lname_substr !~~* '_A%';
--16740

select count(*)
from phone_book_substr
where lname_substr !~~* '_b%';
--14148

select count(*)
from phone_book_substr
where lname_substr !~~* '_B%';
--14148

select count(*)
from phone_book_substr
where lname_substr !~~* 'B%';
--16767

select count(*)
from phone_book_substr
where lname_substr !~~* '_b_';
--14148

RESET ALL;

-- ----------------------------------------------------------------------
-- Test: reg-exp-similar-qry011.sql
-- ----------------------------------------------------------------------

--select count(*)
--from phone_book_substr
--;
--17388
---------------------------------------------------------
-- SIMILAR TO
-- | is equivalent to LOGICAL "OR"

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO '(A|B)%';
--1242

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO 'A%';
--621

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO 'B%';
--621

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO 'A%'
OR lname_substr SIMILAR TO 'B%';
--1242
-------------------------------------------
-- * denotes repetition of the previous item zero or more times.

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO '(A|B)*';
--0

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO 'A*';
--0

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO 'B*';
--0

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO 'A*'
OR lname_substr SIMILAR TO 'B*';
--0
-------------------------------------------
-- + denotes repetition of the previous item zero or more times.

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO '(A|B)+';
--0

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO 'A+';
--0

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO 'B+';
--0

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO 'A+'
OR lname_substr SIMILAR TO 'B+';
--0
-------------------------------------------
-- [...] denotes specifies a character class.

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO '([A...]|[B...])';
--0

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO '[A...]';
--0

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO '[B...]';
--0

select count(*)
from phone_book_substr
where lname_substr SIMILAR TO '[A...]'
OR lname_substr SIMILAR TO '[B...]';
--0

RESET ALL;

-- ----------------------------------------------------------------------
-- Test: reg-exp-tilde-like-qry013.sql
-- ----------------------------------------------------------------------

--select count(*)
--from phone_book_substr
--;
--17388
---------------------------------------------------------
-- !~~ is equivalent to NOT LIKE

select count(*)
from phone_book_substr
where lname_substr ~~ 'A%';
--621

select count(*)
from phone_book_substr
where lname_substr ~~ '_A%';
--0

select count(*)
from phone_book_substr
where lname_substr ~~ '_b%';
--3240

select count(*)
from phone_book_substr
where lname_substr ~~ '_B%';
--0

select count(*)
from phone_book_substr
where lname_substr ~~ 'B%';
--621

select count(*)
from phone_book_substr
where lname_substr ~~ '_b_';
--3240

RESET ALL;

-- ----------------------------------------------------------------------
-- Test: reg-exp-tilde-ilike-qry014.sql
-- ----------------------------------------------------------------------

--select count(*)
--from phone_book_substr
--;
--17388
---------------------------------------------------------
-- ~~* is equivalent to ILIKE

select count(*)
from phone_book_substr
where lname_substr ~~* 'A%';
--621

select count(*)
from phone_book_substr
where lname_substr ~~* '_A%';
--648

select count(*)
from phone_book_substr
where lname_substr ~~* '_b%';
--3240

select count(*)
from phone_book_substr
where lname_substr ~~* '_B%';
--3240

select count(*)
from phone_book_substr
where lname_substr ~~* 'B%';
--621

select count(*)
from phone_book_substr
where lname_substr ~~* '_b_';
--3240

RESET ALL;

-- ----------------------------------------------------------------------
-- Test: drop-regexp.sql
-- ----------------------------------------------------------------------

drop table phone_book;
drop table phone_book_substr;
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema qp_regexp cascade;
-- end_ignore
RESET ALL;
