select decrypt(encrypt('abcd1234', 'key1', 'bf'), 'key1', 'bf');
select decrypt(encrypt('abcd1234', 'fooz', 'bf-cbc/pad:pkcs'), 'fooz', 'bf-cbc/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'bf-ecb/pad:pkcs'), 'fooz', 'bf-ecb/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'bf-cbc/pad:none'), 'fooz', 'bf-cbc/pad:none');
select decrypt(encrypt('abcd1234', 'fooz', 'bf-ecb/pad:none'), 'fooz', 'bf-ecb/pad:none');
select decrypt(encrypt('abcd1234', 'fooz', 'bf-cfb/pad:pkcs'), 'fooz', 'bf-cfb/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'bf-cfb/pad:none'), 'fooz', 'bf-cfb/pad:none');

select decrypt(encrypt('abcd1234', 'key1', 'blowfish'), 'key1', 'blowfish');
select decrypt(encrypt('abcd1234', 'fooz', 'blowfish-cbc/pad:pkcs'), 'fooz', 'blowfish-cbc/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'blowfish-ecb/pad:pkcs'), 'fooz', 'blowfish-ecb/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'blowfish-cbc/pad:none'), 'fooz', 'blowfish-cbc/pad:none');
select decrypt(encrypt('abcd1234', 'fooz', 'blowfish-ecb/pad:none'), 'fooz', 'blowfish-ecb/pad:none');
select decrypt(encrypt('abcd1234', 'fooz', 'blowfish-cfb/pad:pkcs'), 'fooz', 'blowfish-cfb/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'blowfish-cfb/pad:none'), 'fooz', 'blowfish-cfb/pad:none');

select decrypt(encrypt('abcd1234', 'key1', 'aes'), 'key1', 'aes');
select decrypt(encrypt('abcd1234', 'fooz', 'aes-cbc/pad:pkcs'), 'fooz', 'aes-cbc/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'aes-ecb/pad:pkcs'), 'fooz', 'aes-ecb/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'aes-cbc/pad:none'), 'fooz', 'aes-cbc/pad:none');
select decrypt(encrypt('abcd1234', 'fooz', 'aes-ecb/pad:none'), 'fooz', 'aes-ecb/pad:none');

select decrypt(encrypt('abcd1234', 'key1', '3des'), 'key1', '3des');
select decrypt(encrypt('abcd1234', 'fooz', '3des-cbc/pad:pkcs'), 'fooz', '3des-cbc/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', '3des-ecb/pad:pkcs'), 'fooz', '3des-ecb/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', '3des-cbc/pad:none'), 'fooz', '3des-cbc/pad:none');
select decrypt(encrypt('abcd1234', 'fooz', '3des-ecb/pad:none'), 'fooz', '3des-ecb/pad:none');

select decrypt(encrypt('abcd1234', 'key1', 'rijndael'), 'key1', 'rijndael');
select decrypt(encrypt('abcd1234', 'fooz', 'rijndael-cbc/pad:pkcs'), 'fooz', 'rijndael-cbc/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'rijndael-ecb/pad:pkcs'), 'fooz', 'rijndael-ecb/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'rijndael-cbc/pad:none'), 'fooz', 'rijndael-cbc/pad:none');
select decrypt(encrypt('abcd1234', 'fooz', 'rijndael-ecb/pad:none'), 'fooz', 'rijndael-ecb/pad:none');

select decrypt(encrypt('abcd1234', 'key1', 'cast5'), 'key1', 'cast5');
select decrypt(encrypt('abcd1234', 'fooz', 'cast5-cbc/pad:pkcs'), 'fooz', 'cast5-cbc/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'cast5-ecb/pad:pkcs'), 'fooz', 'cast5-ecb/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'cast5-cbc/pad:none'), 'fooz', 'cast5-cbc/pad:none');
select decrypt(encrypt('abcd1234', 'fooz', 'cast5-ecb/pad:none'), 'fooz', 'cast5-ecb/pad:none');

select decrypt(encrypt('abcd1234', 'key1', 'des'), 'key1', 'des');
select decrypt(encrypt('abcd1234', 'fooz', 'des-cbc/pad:pkcs'), 'fooz', 'des-cbc/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'des-ecb/pad:pkcs'), 'fooz', 'des-ecb/pad:pkcs');
select decrypt(encrypt('abcd1234', 'fooz', 'des-cbc/pad:none'), 'fooz', 'des-cbc/pad:none');
select decrypt(encrypt('abcd1234', 'fooz', 'des-ecb/pad:none'), 'fooz', 'des-ecb/pad:none');
