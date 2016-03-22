--
-- NAME
-- all inputs are silently truncated at NAMEDATALEN-1 (63) characters
--

-- fixed-length by reference
SELECT name 'name string' = name 'name string' AS "True";

SELECT name 'name string' = name 'name string ' AS "False";

--
--
--

CREATE TABLE NAME_TBL(f1 name);

INSERT INTO NAME_TBL(f1) VALUES ('1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR');

INSERT INTO NAME_TBL(f1) VALUES ('1234567890abcdefghijklmnopqrstuvwxyz1234567890abcdefghijklmnopqr');

INSERT INTO NAME_TBL(f1) VALUES ('asdfghjkl;');

INSERT INTO NAME_TBL(f1) VALUES ('343f%2a');

INSERT INTO NAME_TBL(f1) VALUES ('d34aaasdf');

INSERT INTO NAME_TBL(f1) VALUES ('');

INSERT INTO NAME_TBL(f1) VALUES ('1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ');


SELECT '' AS seven, * FROM NAME_TBL order by f1;

SELECT '' AS six, c.f1 FROM NAME_TBL c WHERE c.f1 <> '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR' order by f1;

SELECT '' AS one, c.f1 FROM NAME_TBL c WHERE c.f1 = '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR' order by f1;

SELECT '' AS three, c.f1 FROM NAME_TBL c WHERE c.f1 < '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR' order by f1;

SELECT '' AS four, c.f1 FROM NAME_TBL c WHERE c.f1 <= '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR' order by f1;

SELECT '' AS three, c.f1 FROM NAME_TBL c WHERE c.f1 > '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR' order by f1;

SELECT '' AS four, c.f1 FROM NAME_TBL c WHERE c.f1 >= '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR' order by f1;

SELECT '' AS seven, c.f1 FROM NAME_TBL c WHERE c.f1 ~ '.*' order by f1;

SELECT '' AS zero, c.f1 FROM NAME_TBL c WHERE c.f1 !~ '.*' order by f1;

SELECT '' AS three, c.f1 FROM NAME_TBL c WHERE c.f1 ~ '[0-9]' order by f1;

SELECT '' AS two, c.f1 FROM NAME_TBL c WHERE c.f1 ~ '.*asdf.*' order by f1;

DROP TABLE NAME_TBL;

-- alter external protocol's name

CREATE OR REPLACE FUNCTION write_to_file() RETURNS integer as '$libdir/gpextprotocol.so', 'demoprot_export' LANGUAGE C STABLE NO SQL;
CREATE OR REPLACE FUNCTION read_from_file() RETURNS integer as '$libdir/gpextprotocol.so', 'demoprot_import' LANGUAGE C STABLE NO SQL;
CREATE TRUSTED PROTOCOL demoprot (readfunc = 'read_from_file', writefunc = 'write_to_file');

ALTER PROTOCOL demoprot RENAME TO demoprot2;

select count(distinct ptcname) from (
        select ptcname AS ptcname from gp_dist_random('pg_extprotocol')
) all_segments;

DROP PROTOCOL if exists demoprot;
DROP PROTOCOL if exists demoprot2;
