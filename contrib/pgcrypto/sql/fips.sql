ALTER DATABASE contrib_regression SET pgcrypto.fips TO on;
\c contrib_regression

SHOW pgcrypto.fips;

CREATE TABLE fipstest (data text, res text, salt text);
INSERT INTO fipstest VALUES ('password', '', '');

SELECT 'Test digest md5: EXPECTED ERROR FAIL FIPS' as comment;
SELECT digest('santa claus', 'md5');

SELECT 'Test digest sha256: EXPECTED PASS' as comment;
SELECT digest('santa claus', 'sha256');

SELECT 'Test hmac md5: EXPECTED ERROR FAIL FIPS' as comment;
SELECT hmac('santa claus', 'aaa', 'md5');

SELECT 'Test hmac sha256: EXPECTED PASS' as comment;
SELECT hmac('santa claus', 'aaa', 'sha256');

SELECT 'Test gen_salt : EXPECTED FAIL FIPS' as comment;
UPDATE fipstest SET salt = gen_salt('md5');

SELECT 'Test crypt : EXPECTED FAIL FIPS' as comment;
UPDATE fipstest SET res = crypt(data, salt);
SELECT res = crypt(data, res) AS "worked" FROM fipstest;

SELECT 'Test pgp : EXPECTED PASS' as comment;
select pgp_sym_decrypt(pgp_sym_encrypt('santa clause', 'mypass', 'cipher-algo=aes256'), 'mypass');

SELECT 'Test pgp : EXPECTED FAIL FIPS' as comment;
select pgp_sym_decrypt(pgp_sym_encrypt('santa clause', 'mypass', 'cipher-algo=bf'), 'mypass');

SELECT 'Test raw encrypt : EXPECTED PASS' as comment;
SELECT encrypt('santa claus', 'mypass', 'aes') as raw_aes;

DROP TABLE fipstest;
