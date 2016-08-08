-- start_ignore
drop table crypto;
-- end_ignore

-- digest and encode example
SELECT encode(digest('test', 'sha1'), 'hex');
SELECT encode(digest(E'\\\\134', 'sha1'), 'hex');


-- hmac
select hmac('test','test','sha1');
select hmac(E'\\\\134',E'\\\\134','sha1');

-- encrypt
select count(encrypt(E'\\\\134', E'\\\\134', 'bf'));

-- pgp_sym_encrypt
select count(pgp_sym_encrypt('test', 'bf'));
select count(pgp_sym_encrypt('test', 'aes128'));
select count(pgp_sym_encrypt('test', 'aes192'));
select count(pgp_sym_encrypt('test', 'aes256'));

-- encrypt, decrypt
CREATE TABLE crypto (
id SERIAL PRIMARY KEY,
title VARCHAR(50),
crypted_content BYTEA
);
INSERT INTO crypto VALUES (1,'test1',encrypt('daniel', 'fooz', 'aes'));
INSERT INTO crypto VALUES (2,'test2',encrypt('struck', 'fooz', 'aes'));
INSERT INTO crypto VALUES (3,'test3',encrypt('konz', 'fooz', 'aes'));
SELECT * FROM crypto;
SELECT *,decrypt(crypted_content, 'fooz', 'aes') FROM crypto; 
SELECT *,decrypt(crypted_content, 'fooz', 'aes') FROM crypto WHERE
decrypt(crypted_content, 'fooz', 'aes') = 'struck'; 


-- encrypt_iv decrypt_iv

-- gen_random_byptes
select count(gen_random_bytes(4));


-- pgp_sym_encrypt/decrypt
select count(pgp_sym_encrypt('test','test'));
select pgp_sym_decrypt(pgp_sym_encrypt('test','test'), 'test');
select pgp_sym_decrypt(pgp_sym_encrypt('test','test'), 'test', 'compress-algo=1, cipher-algo=aes256');
select count(pgp_sym_encrypt('test','test','compress-algo=1, cipher-algo=aes256'));

-- pgp_sym_encrypt_bytea/decrypt
select count(pgp_sym_encrypt_bytea(E'\\\\123', 'test'));
select count(pgp_sym_encrypt_bytea(E'\\\\123', 'test', 'compress-algo=1, cipher-algo=aes256'));
select pgp_sym_decrypt_bytea(pgp_sym_encrypt_bytea(E'\\\\123', 'test', 'compress-algo=1, cipher-algo=aes256'), 
'test');
select pgp_sym_decrypt_bytea(pgp_sym_encrypt_bytea(E'\\\\123', 'test', 'compress-algo=1, cipher-algo=aes256'), 'test', 'compress-algo=1, cipher-algo=aes256');

