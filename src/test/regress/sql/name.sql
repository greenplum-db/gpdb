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


SELECT '' AS seven, * FROM NAME_TBL;

SELECT '' AS six, c.f1 FROM NAME_TBL c WHERE c.f1 <> '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR';

SELECT '' AS one, c.f1 FROM NAME_TBL c WHERE c.f1 = '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR';

SELECT '' AS three, c.f1 FROM NAME_TBL c WHERE c.f1 < '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR';

SELECT '' AS four, c.f1 FROM NAME_TBL c WHERE c.f1 <= '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR';

SELECT '' AS three, c.f1 FROM NAME_TBL c WHERE c.f1 > '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR';

SELECT '' AS four, c.f1 FROM NAME_TBL c WHERE c.f1 >= '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQR';

SELECT '' AS seven, c.f1 FROM NAME_TBL c WHERE c.f1 ~ '.*';

SELECT '' AS zero, c.f1 FROM NAME_TBL c WHERE c.f1 !~ '.*';

SELECT '' AS three, c.f1 FROM NAME_TBL c WHERE c.f1 ~ '[0-9]';

SELECT '' AS two, c.f1 FROM NAME_TBL c WHERE c.f1 ~ '.*asdf.*';

DROP TABLE NAME_TBL;
