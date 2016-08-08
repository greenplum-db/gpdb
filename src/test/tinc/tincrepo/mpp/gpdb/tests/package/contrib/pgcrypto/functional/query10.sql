select decrypt_iv(encrypt_iv('abcd1234', 'key1', '0', 'bf'), 'key1', '0', 'bf');

select decrypt_iv(encrypt_iv('abcd1234', 'fooz', '0', 'bf-cbc/pad:pkcs'), 'fooz', '0','bf-cbc/pad:pkcs');

select decrypt_iv(encrypt_iv('abcd1234', 'fooz', '0', 'bf-ecb/pad:pkcs'), 'fooz', '0', 'bf-ecb/pad:pkcs');

select decrypt_iv(encrypt_iv('abcd1234', 'fooz', '0','bf-cbc/pad:none'), 'fooz', '0','bf-cbc/pad:none');

select decrypt_iv(encrypt_iv('abcd1234', 'fooz', '0','bf-ecb/pad:none'), 'fooz', '0','bf-ecb/pad:none');

select decrypt_iv(encrypt_iv('abcd1234', 'key1', '0', 'aes'), 'key1', '0','aes');

select decrypt_iv(encrypt_iv('abcd1234', 'fooz', '0', 'aes-cbc/pad:pkcs'), 'fooz', '0','aes-cbc/pad:pkcs');

select decrypt_iv(encrypt_iv('abcd1234', 'fooz', '0','aes-ecb/pad:pkcs'), 'fooz', '0','aes-ecb/pad:pkcs');

select decrypt_iv(encrypt_iv('abcd1234', 'fooz', '0', 'aes-cbc/pad:none'), 'fooz', '0', 'aes-cbc/pad:none');

select decrypt_iv(encrypt_iv('abcd1234', 'fooz', '0', 'aes-ecb/pad:none'), 'fooz', '0','aes-ecb/pad:none');
