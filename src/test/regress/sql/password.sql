--
-- Tests for password verifiers
--

-- Tests for GUC password_encryption
SET password_hash_algorithm = 'novalue'; -- error
SET password_encryption = on; -- ok
SET password_hash_algorithm = 'md5'; -- ok
SET password_encryption = off; -- ok
SET password_hash_algorithm = 'scram-sha-256'; -- ok

-- consistency of password entries
SET password_encryption = off;
CREATE ROLE regress_passwd1 PASSWORD 'role_pwd1';
SET password_encryption = 'on';
SET password_hash_algorithm = 'md5';
CREATE ROLE regress_passwd2 PASSWORD 'role_pwd2';
SET password_encryption = 'on';
CREATE ROLE regress_passwd3 PASSWORD 'role_pwd3';
SET password_hash_algorithm = 'scram-sha-256';
CREATE ROLE regress_passwd4 PASSWORD 'role_pwd4';
SET password_encryption = 'off';
CREATE ROLE regress_passwd5 PASSWORD NULL;

-- check list of created entries
--
-- The scram verifier will look something like:
-- SCRAM-SHA-256$4096:E4HxLGtnRzsYwg==$6YtlR4t69SguDiwFvbVgVZtuz6gpJQQqUMZ7IQJK5yI=:ps75jrHeYU4lXCcXI4O8oIdJ3eO8o2jirjruw9phBTo=
--
-- Since the salt is random, the exact value stored will be different on every test
-- run. Use a regular expression to mask the changing parts.
SELECT rolname, regexp_replace(rolpassword, '(SCRAM-SHA-256)\$(\d+):([a-zA-Z0-9+/=]+)\$([a-zA-Z0-9+=/]+):([a-zA-Z0-9+/=]+)', '\1$\2:<salt>$<storedkey>:<serverkey>') as rolpassword_masked
    FROM pg_authid
    WHERE rolname LIKE 'regress_passwd%'
    ORDER BY rolname, rolpassword;

-- Rename a role
ALTER ROLE regress_passwd3 RENAME TO regress_passwd3_new;
-- md5 entry should have been removed
SELECT rolname, rolpassword
    FROM pg_authid
    WHERE rolname LIKE 'regress_passwd3_new'
    ORDER BY rolname, rolpassword;
ALTER ROLE regress_passwd3_new RENAME TO regress_passwd3;

-- ENCRYPTED and UNENCRYPTED passwords
ALTER ROLE regress_passwd1 UNENCRYPTED PASSWORD 'foo'; -- unencrypted
ALTER ROLE regress_passwd2 UNENCRYPTED PASSWORD 'md5dfa155cadd5f4ad57860162f3fab9cdb'; -- encrypted with MD5
SET password_hash_algorithm = 'md5';
ALTER ROLE regress_passwd3 ENCRYPTED PASSWORD 'foo'; -- encrypted with MD5

ALTER ROLE regress_passwd4 ENCRYPTED PASSWORD 'SCRAM-SHA-256$4096:VLK4RMaQLCvNtQ==$6YtlR4t69SguDiwFvbVgVZtuz6gpJQQqUMZ7IQJK5yI=:ps75jrHeYU4lXCcXI4O8oIdJ3eO8o2jirjruw9phBTo='; -- client-supplied SCRAM verifier, use as it is

SET password_hash_algorithm = 'scram-sha-256';
ALTER ROLE  regress_passwd5 ENCRYPTED PASSWORD 'foo'; -- create SCRAM verifier
CREATE ROLE regress_passwd6 ENCRYPTED PASSWORD 'md53725413363ab045e20521bf36b8d8d7f'; -- encrypted with MD5, use as it is

SELECT rolname, regexp_replace(rolpassword, '(SCRAM-SHA-256)\$(\d+):([a-zA-Z0-9+/=]+)\$([a-zA-Z0-9+=/]+):([a-zA-Z0-9+/=]+)', '\1$\2:<salt>$<storedkey>:<serverkey>') as rolpassword_masked
    FROM pg_authid
    WHERE rolname LIKE 'regress_passwd%'
    ORDER BY rolname, rolpassword;

-- An empty password is not allowed, in any form
CREATE ROLE regress_passwd_empty PASSWORD '';
ALTER ROLE regress_passwd_empty PASSWORD 'md585939a5ce845f1a1b620742e3c659e0a';
ALTER ROLE regress_passwd_empty PASSWORD 'SCRAM-SHA-256$4096:hpFyHTUsSWcR7O9P$LgZFIt6Oqdo27ZFKbZ2nV+vtnYM995pDh9ca6WSi120=:qVV5NeluNfUPkwm7Vqat25RjSPLkGeoZBQs6wVv+um4=';
SELECT rolpassword FROM pg_authid WHERE rolname='regress_passwd_empty';

-- Test with invalid stored and server keys.
--
-- The first is valid, to act as a control. The others have too long
-- stored/server keys. They will be re-hashed.
CREATE ROLE regress_passwd_sha_len0 PASSWORD 'SCRAM-SHA-256$4096:A6xHKoH/494E941doaPOYg==$Ky+A30sewHIH3VHQLRN9vYsuzlgNyGNKCh37dy96Rqw=:COPdlNiIkrsacU5QoxydEuOH6e/KfiipeETb/bPw8ZI=';
CREATE ROLE regress_passwd_sha_len1 PASSWORD 'SCRAM-SHA-256$4096:A6xHKoH/494E941doaPOYg==$Ky+A30sewHIH3VHQLRN9vYsuzlgNyGNKCh37dy96RqwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=:COPdlNiIkrsacU5QoxydEuOH6e/KfiipeETb/bPw8ZI=';
CREATE ROLE regress_passwd_sha_len2 PASSWORD 'SCRAM-SHA-256$4096:A6xHKoH/494E941doaPOYg==$Ky+A30sewHIH3VHQLRN9vYsuzlgNyGNKCh37dy96Rqw=:COPdlNiIkrsacU5QoxydEuOH6e/KfiipeETb/bPw8ZIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=';

-- Check that the invalid verifiers were re-hashed. A re-hashed verifier
-- should not contain the original salt.
SELECT rolname, rolpassword not like '%A6xHKoH/494E941doaPOYg==%' as is_rolpassword_rehashed
    FROM pg_authid
    WHERE rolname LIKE 'regress_passwd_sha_len%'
    ORDER BY rolname;

DROP ROLE regress_passwd1;
DROP ROLE regress_passwd2;
DROP ROLE regress_passwd3;
DROP ROLE regress_passwd4;
DROP ROLE regress_passwd5;
DROP ROLE regress_passwd6;
DROP ROLE regress_passwd_empty;
DROP ROLE regress_passwd_sha_len0;
DROP ROLE regress_passwd_sha_len1;
DROP ROLE regress_passwd_sha_len2;

-- all entries should have been removed
SELECT rolname, rolpassword
    FROM pg_authid
    WHERE rolname LIKE 'regress_passwd%'
    ORDER BY rolname, rolpassword;
