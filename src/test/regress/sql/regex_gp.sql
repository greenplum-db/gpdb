
drop table if exists phone_book;

create table phone_book
(
lname char varying(25),
fname char varying(25),
state char(2),
phone_num bigint
)
distributed by (lname);

\copy public.phone_book from 'data/phone_book.txt' delimiter as '|'

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

--select count(*)
--from phone_book_substr
--;

--17388
---------------------------------------------------------

-- ILIKE  case sensitive LIKE

select count(*)
from phone_book_substr
where lname_substr ILIKE 'a%';
--621

select count(*)
from phone_book_substr
where lname_substr  ILIKE 'A%';
--621

select count(*)
from phone_book_substr
where lname_substr  ILIKE 'Aa';
--0

select count(*)
from phone_book_substr
where lname_substr  ILIKE '_a_';
--648

select count(*)
from phone_book_substr
where lname_substr  ILIKE '_Z_';
--0

select count(*)
from phone_book_substr
where lname_substr  ILIKE 'Abd';
--0

--select count(*)
--from phone_book_substr
--;
--17388
---------------------------------------------------------
-- NOT SIMILAR TO
-- | is equivalent to LOGICAL "OR"

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO '(A|B)%';
--16146

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO 'A%';
--16767

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO 'B%';
--16767

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO 'A%'
AND lname_substr NOT SIMILAR TO 'B%';
--16146
-------------------------------------------
-- * denotes repetition of the previous item zero or more times.

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO '(A|B)*';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO 'A*';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO 'B*';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO 'A*'
AND lname_substr NOT SIMILAR TO 'B*';
--17388
-------------------------------------------
-- + denotes repetition of the previous item zero or more times.

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO '(A|B)+';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO 'A+';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO 'B+';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO 'A+'
AND lname_substr NOT SIMILAR TO 'B+';
--17388
-------------------------------------------
-- [...] denotes specifies a character class.

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO '([A...]|[B...])';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO '[A...]';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO '[B...]';
--17388

select count(*)
from phone_book_substr
where lname_substr NOT SIMILAR TO '[A...]'
AND lname_substr NOT SIMILAR TO '[B...]';
--17388

--select count(*)
--from phone_book_substr
--;
--17388
---------------------------------------------------------
-- ~~ is equivalent to LIKE

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
where lname_substr !~~ 'B%';
--16767

select count(*)
from phone_book_substr
where lname_substr !~~ '_b_';
--14148

-----------------------------------------------------------------
---substring(string from pattern for escape character)    
-- provides extraction of a substring that matches a SQL
-- regular expression. To indicate the part of the pattern 
-- that should be returned on success, the pattern must 
-- contain two occurrences of the escape character 
-- followed by a double quote ("). The
-- text matching the portion of the pattern between these markers 
-- is returned.
----------------------------------------------------------

SELECT SUBSTRING('XY1234Z', 'Y*([0-9]{1,3})');
-- substring 
-----------
-- 123
--(1 row)

SELECT SUBSTRING('XY1234Z', 'Y*?([0-9]{1,3})');
-- substring 
-----------
-- 1
--(1 row)

----------------------------------------------------------
----------------------------------------------------------

--select substring(lname from '%#"Z_y#"%' for '#') IS NOT NULL
--from phone_book_substr
--where substring(lname from '%#"Z_y#"%' for '#') IS NOT NULL
--;

-- returns 27 rows.
--  lname  | substring 
-----------+-----------
-- Zdygahd | Zdy

select lname, substring(lname from '%#"Z_yg_h#"%' for '#')
from phone_book_substr
where substring(lname from '%#"Z_yg_h#"%' for '#') IS NOT NULL
;

-- returns 27 rows.
--  lname  | substring 
---------+-----------
-- Zdygahd | Zdy

--select lname, substring(lname from '%\\"Z_yg_h\\"%' for '\\')
--from phone_book_substr
--where   substring(lname from '%\\"Z_yg_h\\"%' for '\\') IS NOT NULL
--;

-- returns 27 rows.
--  lname  | substring 
---------+-----------
-- Zdygahd | Zdy

--select DISTINCT lname, substring(lname from '%\\"__yg_h\\"%' for '\\')
--from phone_book_substr
--where   substring(lname from '%\\"__yg_h\\"%' for '\\') IS NOT NULL
--;

--dygahd | Adygah
-- Bdygahd | Bdygah
-- Cdygahd | Cdygah
-- Ddygahd | Ddygah
-- Edygahd | Edygah
-- Fdygahd | Fdygah
-- Gdygahd | Gdygah
-- Hdygahd | Hdygah
-- Idygahd | Idygah
-- Jdygahd | Jdygah
-- Kdygahd | Kdygah
-- Keygahd | Keygah
-- Ldygahd | Ldygah
-- Liygahd | Liygah
-- Mdygahd | Mdygah
-- Moygahd | Moygah
-- Ndygahd | Ndygah
-- Nuygahd | Nuygah
-- Odygahd | Odygah
-- Pdygahd | Pdygah
-- Rdygahd | Rdygah
-- Sdygahd | Sdygah
-- Udygahd | Udygah
-- Gdygahd | Gdygah
-- Hdygahd | Hdygah
-- Idygahd | Idygah
-- Jdygahd | Jdygah
-- Kdygahd | Kdygah
-- Keygahd | Keygah
-- Ldygahd | Ldygah
-- Liygahd | Liygah
-- Mdygahd | Mdygah
-- Moygahd | Moygah
-- Ndygahd | Ndygah
-- Nuygahd | Nuygah
-- Odygahd | Odygah
-- Pdygahd | Pdygah
-- Rdygahd | Rdygah
-- Sdygahd | Sdygah
-- Udygahd | Udygah
-- Vdygahd | Vdygah
-- Wdygahd | Wdygah
-- Xdygahd | Xdygah
-- Ydygahd | Ydygah
-- Zdygahd | Zdygah

------------------------------------------------------------------------
-- The function regexp_replace:  Regular Expression Replace.  
-- The syntax for regexp_replace is 
-- regexp_replace(source, pattern, replacement[,flags])
-- The syntax is reg
------------------------------------------------------------------------
select regexp_replace('foobarbaz', 'b..', 'X');

-- regexp_replace 
----------------
-- fooXbaz
--(1 row)

select regexp_replace('foobarbaz', 'b..', 'X', 'g');

-- regexp_replace 
----------------
-- fooXX
--(1 row)

--select regexp_replace('foobarbaz', 'b(..)', E'X\\1Y', 'g');

-- regexp_replace 
----------------
-- fooXarYXazY
--(1 row)

select regexp_replace('Zdygahd', 'yg','GY')
;

-- regexp_replace 
----------------
-- ZdGYahd
--(1 row)

select DISTINCT lname, regexp_replace(lname, '...g..','G')
from phone_book_substr order by lname
;

--  lname  | regexp_replace 
---------+----------------
-- Aadgahd | Gd
-- Abegahd | Gd
-- Abfgahd | Gd
-- Abggahd | Gd
-- Abhgahd | Gd
-- Abigahd | Gd
--.............
--..............
-- Zdugahd | Gd
-- Zdvgahd | Gd
-- Zdwgahd | Gd
-- Zdxgahd | Gd
-- Zdygahd | Gd
-- Zdzgahd | Gd
--(644 rows)

select DISTINCT lname, regexp_replace(lname, '...g...','G')
from phone_book_substr order by lname
;

--  lname  | regexp_replace 
-----------+----------------
-- Aadgahd | G
-- Abegahd | G
-- Abfgahd | G
--...............
--...............
-- Zdxgahd | G
-- Zdygahd | G
-- Zdzgahd | G
--(644 rows)

select DISTINCT lname, regexp_replace(lname, '...g...','...G...')
from phone_book_substr order by lname
;

--  lname  | regexp_replace 
-----------+----------------
-- Aadgahd | ...G...
-- Abegahd | ...G...
--...............
--...............
-- Zdxgahd | ...G...
-- Zdygahd | ...G...
-- Zdzgahd | ...G...
--(644 rows)

select DISTINCT lname, regexp_replace(lname, 'A..g..d','a..G..D')
from phone_book_substr order by lname
;

--  lname  | regexp_replace 
-----------+----------------
-- Aadgahd | a..G..D
-- Abegahd | a..G..D
-- Abfgahd | a..G..D
--...............
--...............
-- Adzgahd | a..G..D
-- Badgahd | Badgahd
-- Bbegahd | Bbegahd
--...............
--...............
-- Zdxgahd | Zdxgahd
-- Zdygahd | Zdygahd
-- Zdzgahd | Zdzgahd
--(644 rows)

select DISTINCT lname, regexp_replace(lname, 'a','Z','ig')
from phone_book_substr order by lname
;
-- replaces 'a' with 'Z' globally. The matching is case-insensitive.
-- flag 'g' is global replacement and flag 'i' is for matching
-- case-insensitive.

--  lname  | regexp_replace 
-----------+----------------
-- Aadgahd | ZZdgZhd
-- Abegahd | ZbegZhd
-- Abfgahd | ZbfgZhd
-- Abggahd | ZbggZhd
--...................
--...................
-- Zdwgahd | ZdwgZhd
-- Zdxgahd | ZdxgZhd
-- Zdygahd | ZdygZhd
-- Zdzgahd | ZdzgZhd
--(644 rows)

-- match 3rd occurance of 'a' (parenthesized subrexpression of the
-- pattern should be inserted.) and replace it with 'Z'
-- \& indicates that the substring matching the entire pattern 
-- shouldbeisered.

--select DISTINCT lname, regexp_replace(lname, 'a(..)','\\3\\&Z','g')
--from phone_book_substr
--;
--  lname  | regexp_replace 
-----------+----------------
-- Aadgahd | AadgZahdZ
-- Abegahd | AbegahdZ
-- Abfgahd | AbfgahdZ
--...................
--...................
-- Zdwgahd | ZdwgahdZ
-- Zdxgahd | ZdxgahdZ
-- Zdygahd | ZdygahdZ
-- Zdzgahd | ZdzgahdZ
--(644 rows)

--select DISTINCT lname, regexp_replace(lname, 'yg','GY')
--from phone_book_substr
--where  substring(lname from '%\\"yg\\"%' for '\\') IS NOT NULL
--;
--  lname  | regexp_replace 
-----------+----------------
-- Adygahd | AdGYahd
-- Bdygahd | BdGYahd
-- Cdygahd | CdGYahd
-- Ddygahd | DdGYahd
-- Edygahd | EdGYahd
--..................
--..................
-- Udygahd | UdGYahd
-- Vdygahd | VdGYahd
-- Wdygahd | WdGYahd
-- Xdygahd | XdGYahd
-- Ydygahd | YdGYahd
-- Zdygahd | ZdGYahd
--(28 rows)

--select DISTINCT lname, regexp_replace(lname, 'yg.h.','GY')
--from phone_book_substr
--where   substring(lname from '%\\"yg_h\\"%' for '\\') IS NOT NULL
--;

--  lname  | regexp_replace 
-----------+----------------
-- Adygahd | AdGY
-- Bdygahd | BdGY
-- Cdygahd | CdGY
-- Ddygahd | DdGY
--........................
--........................
-- Wdygahd | WdGY
-- Xdygahd | XdGY
-- Ydygahd | YdGY
-- Zdygahd | ZdGY
--(28 rows)

-----------------------------------------------------------------------

SELECT regexp_matches('foobarbequebaz', '(bar)(beque)');
-- regexp_matches 
----------------
-- {bar,beque}
--(1 row)

SELECT regexp_matches('foobarbequebazilbarfbonk', '(b[^b]+)(b[^b]+)', 'g');
-- regexp_matches 
----------------
-- {bar,beque}
-- {bazil,barf}
--(2 rows)

SELECT regexp_matches('foobarbequebaz', 'barbeque');
-- regexp_matches 
----------------
-- {barbeque}
--(1 row)

select regexp_matches('Aadgahd', '(ad)');

-- regexp_matches 
----------------
-- {ad}
--(1 row)

select DISTINCT lname, regexp_matches(lname, 'yg')
from phone_book_substr order by lname;

--  lname  | regexp_matches 
-----------+----------------
-- Adygahd | {yg}
-- Bdygahd | {yg}
-- Cdygahd | {yg}
-- Ddygahd | {yg}
-- Edygahd | {yg}
-- ..............
-- ..............
-- Vdygahd | {yg}
-- Wdygahd | {yg}
-- Xdygahd | {yg}
-- Ydygahd | {yg}
-- Zdygahd | {yg}
--(28 rows)

select DISTINCT lname, regexp_matches(lname, 'z_y') from phone_book_substr ;

-- lname | regexp_matches 
-------+----------------
--(0 rows)

select DISTINCT lname, regexp_matches(lname, 'z') from phone_book_substr order by lname;

--  lname  | regexp_matches 
---------+----------------
-- Adzgahd | {z}
-- Bdzgahd | {z}
-- Cdzgahd | {z}
-- Ddzgahd | {z}
--..............
--..............
-- Wdzgahd | {z}
-- Xdzgahd | {z}
-- Ydzgahd | {z}
-- Zdzgahd | {z}
--(28 rows)

select DISTINCT lname, regexp_matches(lname, 'Z') from phone_book_substr order by lname;

--  lname  | regexp_matches 
-----------+----------------
-- Zadgahd | {Z}
-- Zbegahd | {Z}
-- Zbfgahd | {Z}
-- Zbggahd | {Z}
--..............
--..............
-- Zdxgahd | {Z}
-- Zdygahd | {Z}
-- Zdzgahd | {Z}
--(23 rows)


select DISTINCT lname, regexp_matches(lname, 'W') from phone_book_substr order by lname;

--  lname  | regexp_matches 
-----------+----------------
-- Wadgahd | {W}
-- Wbegahd | {W}
-- Wbfgahd | {W}
-- Wbggahd | {W}
--..............
--..............
-- Wdxgahd | {W}
-- Wdygahd | {W}
-- Wdzgahd | {W}
--(23 rows)


select DISTINCT lname, regexp_matches(lname, '^W') from phone_book_substr order by lname;

--  lname  | regexp_matches 
-----------+----------------
-- Wadgahd | {W}
-- Wbegahd | {W}
-- Wbfgahd | {W}
--.............
--..............
-- Wdxgahd | {W}
-- Wdygahd | {W}
-- Wdzgahd | {W}
--(23 rows)

select DISTINCT lname, regexp_matches(lname, '^W_x') from phone_book_substr ;

-- lname | regexp_matches 
-------+----------------
--(0 rows)

select DISTINCT lname, regexp_matches(lname, '^W_x____') from phone_book_substr ;

-- lname | regexp_matches 
-------+----------------
--(0 rows)

select DISTINCT lname, regexp_matches(lname, '^W_x*') from phone_book_substr ;

-- lname | regexp_matches 
-------+----------------
--(0 rows)

select DISTINCT lname, regexp_matches(lname, '^W_x$d') from phone_book_substr ;

-- lname | regexp_matches 
-------+----------------
--(0 rows)

select DISTINCT lname, regexp_matches(lname, '(dad)') from phone_book_substr ;
-- lname | regexp_matches 
-------+----------------
--(0 rows)

--Changed to First Name from Last Name.

select DISTINCT fname, regexp_matches(fname, '(dad)') from phone_book_substr order by lname;
--  fname  | regexp_matches 
---------+----------------
-- Aahdadj | {dad}
-- Abhdadj | {dad}
-- Achdadj | {dad}
-- Adhdadj | {dad}
--................
--................
-- Zuhdadj | {dad}
-- Zwhdadj | {dad}
-- Zxhdadj | {dad}
-- Zyhdadj | {dad}
--(377 rows)

select DISTINCT fname, regexp_matches(fname, '(uh)(ad)') from phone_book_substr ;
-- fname | regexp_matches 
-------+----------------
--(0 rows)


select DISTINCT fname, regexp_matches(fname, '(uh)(dad)') from phone_book_substr order by fname;

--  fname  | regexp_matches 
-----------+----------------
-- Auhdadj | {uh,dad}
-- Buhdadj | {uh,dad}
-- Duhdadj | {uh,dad}
--...................
--...................
-- Xuhdadj | {uh,dad}
-- Yuhdadj | {uh,dad}
-- Zuhdadj | {uh,dad}
--(19 rows)

select DISTINCT fname, regexp_matches(fname, '(uhdad)') from phone_book_substr order by fname;

--  fname  | regexp_matches 
-----------+----------------
-- Auhdadj | {uhdad}
-- Buhdadj | {uhdad}
-- Duhdadj | {uhdad}
--..................
--..................
-- Xuhdadj | {uhdad}
-- Yuhdadj | {uhdad}
-- Zuhdadj | {uhdad}
--(19 rows)

select DISTINCT fname, regexp_matches(fname, '(y)','i') from phone_book_substr order by fname;

--  fname  | regexp_matches 
-----------+----------------
-- Ayhdadj | {y}
-- Byhdadj | {y}
-- Dyhdadj | {y}
--..............
--..............
-- Vyhdadj | {y}
-- Xyhdadj | {y}
-- Yahdadj | {Y}
-- Ybhdadj | {Y}
-- Ychdadj | {Y}
-- Ydhdadj | {Y}
--..............
--..............
-- Yxhdadj | {Y}
-- Yyhdadj | {Y}
-- Zyhdadj | {y}
--(33 rows)

select DISTINCT fname, regexp_matches(fname, '(y|h)','i') from phone_book_substr order by fname;

--  fname  | regexp_matches 
-----------+----------------
-- Aahdadj | {h}
-- Abhdadj | {h}
--..............
-- Bghdadj | {h}
-- Bihdadj | {h}
--..............
-- Yxhdadj | {Y}
-- Yyhdadj | {Y}
-- Zahdadj | {h}
--..............
-- Zxhdadj | {h}
-- Zyhdadj | {y}
--(377 rows)
--
--Exactly one match {1}

select DISTINCT fname, regexp_matches(fname, '(y|h){1}','i') from phone_book_substr order by fname;

--  fname  | regexp_matches 
-----------+----------------
-- Aahdadj | {h}
-- Abhdadj | {h}
-- Achdadj | {h}
-- Adhdadj | {h}
--..............
-- Vyhdadj | {y}
-- Wahdadj | {h}
--..............
-- Xwhdadj | {h}
-- Xxhdadj | {h}
-- Xyhdadj | {y}
-- Yahdadj | {Y}
--..............
-- Yxhdadj | {Y}
-- Yyhdadj | {Y}
-- Zahdadj | {h}
--..............
-- Zwhdadj | {h}
-- Zxhdadj | {h}
-- Zyhdadj | {y}
--(377 rows)

select DISTINCT fname, regexp_matches(fname, '(W){2}','i') from phone_book_substr ;
-- fname | regexp_matches 
-------+----------------
--(0 rows)

--Exactly two consecutive matches {2}.
--Is this right?

select DISTINCT fname, regexp_matches(fname, '(d){2}','i') from phone_book_substr ;
--  fname  | regexp_matches 
---------+----------------
-- Ddhdadj | {d}
--(1 row)

--One or more but not more than two.
--Is this right?

select DISTINCT fname, regexp_matches(fname, '(d){1,2}','i') from phone_book_substr order by fname;

--  fname  | regexp_matches 
---------+----------------
-- Aahdadj | {d}
-- Abhdadj | {d}
-- Achdadj | {d}
--..............
-- Dahdadj | {D}
-- Dbhdadj | {D}
-- Dchdadj | {D}
-- Ddhdadj | {d}
-- Dehdadj | {D}
-- Dfhdadj | {D}
--..............
-- Zwhdadj | {d}
-- Zxhdadj | {d}
-- Zyhdadj | {d}
--(424 rows)
--
--Two or more matches {2,}

select DISTINCT lname, regexp_matches(lname, '(d){2,}','i') from phone_book_substr order by lname;
--  lname  | regexp_matches 
-----------+----------------
-- Ddsgahd | {d}
-- Ddtgahd | {d}
-- Ddugahd | {d}
-- Ddvgahd | {d}
-- Ddwgahd | {d}
-- Ddxgahd | {d}
-- Ddygahd | {d}
-- Ddzgahd | {d}
--(8 rows)
--
--3 or more matches.
--Is this right?

select DISTINCT lname, regexp_matches(lname, '(d){3,}','i') from phone_book_substr ;

-- lname | regexp_matches 
-------+----------------
--(0 rows)

--Exactly 3 matches

select DISTINCT lname, regexp_matches(lname, '(d){3}','i') from phone_book_substr ;
-- lname | regexp_matches 
-------+----------------
--(0 rows)

--Two or more but not more than 3 matches.

select DISTINCT lname, regexp_matches(lname, '(d){2,3}','i') from phone_book_substr order by lname;
--  lname  | regexp_matches 
---------+----------------
-- Ddsgahd | {d}
-- Ddtgahd | {d}
-- Ddugahd | {d}
-- Ddvgahd | {d}
-- Ddwgahd | {d}
-- Ddxgahd | {d}
-- Ddygahd | {d}
-- Ddzgahd | {d}
--(8 rows)
-- 
--One or more but not more than 2 matches.
--Is this right?

select DISTINCT lname, regexp_matches(lname, '(d){1,2}','i') from phone_book_substr order by lname;
--  lname  | regexp_matches
-----------+----------------
-- Aadgahd | {d}
-- Abegahd | {d}
-- Abfgahd | {d}
--..............
-- Ddvgahd | {d}
-- Ddwgahd | {d}
-- Ddxgahd | {d}
--..............
-- Zdxgahd | {d}
-- Zdygahd | {d}
-- Zdzgahd | {d}
--(644 rows)

select DISTINCT lname, regexp_matches(lname, '(d.d)','i') from phone_book_substr ;

--  lname  | regexp_matches 
---------+----------------
-- Dadgahd | {Dad}
--(1 row)

select DISTINCT lname, regexp_matches(lname, '(g.h)', 'i') from phone_book_substr order by lname;

--  lname  | regexp_matches 
-----------+----------------
-- Aadgahd | {gah}
-- Abegahd | {gah}
-- Abfgahd | {gah}
--................
-- Zdxgahd | {gah}
-- Zdygahd | {gah}
-- Zdzgahd | {gah}
--(644 rows)

select DISTINCT lname, regexp_matches(lname, '[zh]', 'i') from phone_book_substr order by lname;

--  lname  | regexp_matches 
-----------+----------------
-- Aadgahd | {h}
-- Abegahd | {h}
-- Abfgahd | {h}
--..............
-- Gdygahd | {h}
-- Gdzgahd | {z}
-- Hadgahd | {H}
-- Hbegahd | {H}
--..............
-- Ydygahd | {h}
-- Ydzgahd | {z}
-- Zadgahd | {Z}
--..............
-- Zdxgahd | {Z}
-- Zdygahd | {Z}
-- Zdzgahd | {Z}
--(644 rows)

select DISTINCT fname, regexp_matches(fname, '(h){2}','i') from phone_book_substr ;

-- fname | regexp_matches 
-------+----------------
--(0 rows)

--Two or more matches.

select DISTINCT lname, regexp_matches(lname, '(h){2,}','i') from phone_book_substr ;
-- lname | regexp_matches 
-------+----------------
--(0 rows)


select DISTINCT fname, regexp_matches(fname, '(x){2}','i') from phone_book_substr ;

--  fname  | regexp_matches 
-----------+----------------
-- Xxhdadj | {x}
--(1 row)

select DISTINCT fname, regexp_matches(fname, '(x){3}','i') from phone_book_substr ;

-- fname | regexp_matches 
-------+----------------
--(0 rows)

select DISTINCT lname, regexp_matches(lname, '(c){2}','i') from phone_book_substr order by lname;

--  lname  | regexp_matches 
-----------+----------------
-- Ccjgahd | {c}
-- Cckgahd | {c}
-- Cclgahd | {c}
-- Ccmgahd | {c}
-- Ccngahd | {c}
-- Ccogahd | {c}
-- Ccpgahd | {c}
-- Ccqgahd | {c}
-- Ccrgahd | {c}
--(9 rows)


-----------------------------------------------------------------------
--Constraints
-----------
--re - Regular Expression.

--Constraint	Description
--------------------------------------------------------------------------
-- ^ 	matches at the beginning of the string
-- $ 	matches at the end of the string
-- (?=re) 	positive lookahead matches at any point where a substring matching 
-- 	re begins (AREs only)
-- (?!re) 	negative lookahead matches at any point where no substring matching 
-- 	re begins (AREs only) 
-----------------------------------------------------------------------
select DISTINCT lname, regexp_matches(lname, '^z', 'i') from phone_book_substr order by lname;

--  lname  | regexp_matches 
-----------+----------------
-- Zadgahd | {Z}
-- Zbegahd | {Z}
--..............
-- Zdygahd | {Z}
-- Zdzgahd | {Z}
--(23 rows)

select DISTINCT lname, regexp_matches(lname, '$z', 'i') from phone_book_substr;

-- lname | regexp_matches 
-------+----------------
--(0 rows)

select DISTINCT lname, regexp_matches(lname, 'z$') from phone_book_substr;

-- lname | regexp_matches 
-------+----------------
--(0 rows)


select DISTINCT lname, regexp_matches(lname, 'd$') from phone_book_substr order by lname;

--  lname  | regexp_matches 
---------+----------------
-- Aadgahd | {d}
-- Abegahd | {d}
-- Abfgahd | {d}
--................
-- Zdygahd | {d}
-- Zdzgahd | {d}
--(644 rows)
--
--(?=re) 	positive lookahead matches at any point where a substring matching 
--	re begins (AREs only)

--select DISTINCT lname, regexp_matches(lname, '(?=zd)', 'i') from phone_book_substr;

--  lname  | regexp_matches 
-----------+----------------
-- Zdsgahd | {""}
-- Zdtgahd | {""}
-- Zdugahd | {""}
-- Zdvgahd | {""}
-- Zdwgahd | {""}
-- Zdxgahd | {""}
-- Zdygahd | {""}
-- Zdzgahd | {""}
--(8 rows)

--select DISTINCT lname, regexp_matches(lname, '(?=wg)', 'i') from phone_book_substr;

--  lname  | regexp_matches 
-----------+----------------
-- Adwgahd | {""}
-- Bdwgahd | {""}
--................
-- Xdwgahd | {""}
-- Ydwgahd | {""}
-- Zdwgahd | {""}
--(28 rows)

--select DISTINCT lname, regexp_matches(lname, '(?=Ad)', 'i') from phone_book_substr;

--  lname  | regexp_matches 
-----------+----------------
-- Aadgahd | {""}
-- Adsgahd | {""}
-- Adtgahd | {""}
--................
-- Yadgahd | {""}
-- Zadgahd | {""}
--(32 rows)
--
--
--(?!re) 	negative lookahead matches at any point where no substring matching 
--	re begins (AREs only) 

--select DISTINCT lname, regexp_matches(lname, '(?!Ad)', 'i') from phone_book_substr;


--  lname  | regexp_matches 
-----------+----------------
-- Aadgahd | {""}
-- Abegahd | {""}
--...............
-- Zdygahd | {""}
-- Zdzgahd | {""}
--(644 rows)

--select DISTINCT lname, regexp_matches(lname, '(?!hd)', 'i') from phone_book_substr;

--  lname  | regexp_matches 
-----------+----------------
-- Aadgahd | {""}
-- Abegahd | {""}
--...............
-- Zdxgahd | {""}
-- Zdygahd | {""}
-- Zdzgahd | {""}
--(644 rows)

-----------------------------------------------------------------------
-----------------------------------------------------------------------

--where  substring(lname from '%\\"yg\\"%' for '\\') IS NOT NULL
--;

-----------------------------------------------------------------------
-- Function regexp_split_to_table returns all the captured substrings
-- that match with the pattern.
-- select regexp_split_to_table(string, pattern [,flags])');
-- this function splits a string using a POSIX regular expression 
-- pattern as a delimiter.

select foo from regexp_split_to_table('the quick brown fox jumped over the lazy dog',E'\\\s+') AS foo;

SELECT foo FROM regexp_split_to_table('the quick brown fox', E'\\s*') AS foo;

-----------------------------------------------------------------------
----------------------------------------------------------
---substring(string from pattern for escape character)    
-- provides extraction of a substring that matches a SQL
-- regular expression. To indicate the part of the pattern 
-- that should be returned on success, the pattern must 
-- contain two occurrences of the escape character 
-- followed by a double quote ("). The
-- text matching the portion of the pattern between these markers 
-- is returned.
----------------------------------------------------------
select substring('foobar' from 'foo');
--returns foo
select substring('foobar' from 'oba');
--returns oba
select substring('foobar' from 'oo%');
--returns nothing
--select substring('foobar' from '%"o_b#"%' for '#');
--returns nothing
select substring('foobar' from '%#"o_b#"%' for '#');
--returns oob
select count(substring(lname from '%#"o_b#"%' for '#'))
from phone_book_substr;
--0
select count(substring(lname from '%#"Ab#"%' for '#'))
from phone_book_substr;
--135
select substring(lname from '%#"Ab#"%' for '#')
from phone_book_substr
where substring(lname from '%#"Ab#"%' for '#') IS NOT NULL;
--returns 'Ab' 135 times.

select substring(lname from '%#"Zd#"%' for '#')
from phone_book_substr
where substring(lname from '%#"Zd#"%' for '#') IS NOT NULL
;
--returns 'Zd' 216 times.
select substring(lname from '%#"Z_k#"%' for '#')
from phone_book_substr
where substring(lname from '%#"Z_k#"%' for '#') IS NOT NULL
;
--returns 'Zck' 27 times.

select lname, substring(lname from '%#"Z_y#"%' for '#')
from phone_book_substr
where substring(lname from '%#"Z_y#"%' for '#') IS NOT NULL
;

-- returns 27 rows.
--  lname  | substring 
---------+-----------
-- Zdygahd | Zdy

select lname, substring(lname from '%#"Z_yg_h#"%' for '#')
from phone_book_substr
where substring(lname from '%#"Z_yg_h#"%' for '#') IS NOT NULL
;

-- returns 27 rows.
--  lname  | substring 
---------+-----------
-- Zdygahd | Zdy

--select lname, substring(lname from '%\\"Z_yg_h\\"%' for '\\')
--from phone_book_substr
--where   substring(lname from '%\\"Z_yg_h\\"%' for '\\') IS NOT NULL
--;

-- returns 27 rows.
--  lname  | substring 
---------+-----------
-- Zdygahd | Zdy

--select DISTINCT lname, substring(lname from '%\\"__yg_h\\"%' for '\\')
--from phone_book_substr
--where   substring(lname from '%\\"__yg_h\\"%' for '\\') IS NOT NULL
--;

--dygahd | Adygah
-- Bdygahd | Bdygah
-- Cdygahd | Cdygah
-- Ddygahd | Ddygah
-- Edygahd | Edygah
-- Fdygahd | Fdygah
-- Gdygahd | Gdygah
-- Hdygahd | Hdygah
-- Idygahd | Idygah
-- Jdygahd | Jdygah
-- Kdygahd | Kdygah
-- Keygahd | Keygah
-- Ldygahd | Ldygah
-- Liygahd | Liygah
-- Mdygahd | Mdygah
-- Moygahd | Moygah
-- Ndygahd | Ndygah
-- Nuygahd | Nuygah
-- Odygahd | Odygah
-- Pdygahd | Pdygah
-- Rdygahd | Rdygah
-- Sdygahd | Sdygah
-- Udygahd | Udygah
-- Gdygahd | Gdygah
-- Hdygahd | Hdygah
-- Idygahd | Idygah
-- Jdygahd | Jdygah
-- Kdygahd | Kdygah
-- Keygahd | Keygah
-- Ldygahd | Ldygah
-- Liygahd | Liygah
-- Mdygahd | Mdygah
-- Moygahd | Moygah
-- Ndygahd | Ndygah
-- Nuygahd | Nuygah
-- Odygahd | Odygah
-- Pdygahd | Pdygah
-- Rdygahd | Rdygah
-- Sdygahd | Sdygah
-- Udygahd | Udygah
-- Vdygahd | Vdygah
-- Wdygahd | Wdygah
-- Xdygahd | Xdygah
-- Ydygahd | Ydygah
-- Zdygahd | Zdygah


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


select lname_substr 
from phone_book_substr 
where lname_substr similar to '(A|B)%' group by 1 order by 1;

select lname_substr 
from phone_book_substr 
where lname_substr similar to '(A|B)%' group by 1 order by 1;

--select array(select fname 
--		from phone_book_substr 
--		where lname_substr like '%y%');

select lname_substr 
from phone_book_substr 
where lname_substr like 'A%' group by 1 order by 1;

select lname_substr 
from phone_book_substr 
where lname_substr similar to '(A|B)%' group by 1 order by 1;

drop table phone_book;
drop table phone_book_substr;
