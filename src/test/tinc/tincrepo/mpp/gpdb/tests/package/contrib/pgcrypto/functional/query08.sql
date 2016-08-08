select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 'cipher-algo=aes'), '#2x@', 'cipher-algo=aes');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 'cipher-algo=aes128'), '#2x@', 'cipher-algo=aes128');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 'cipher-algo=aes256'), '#2x@', 'cipher-algo=aes256');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 'cipher-algo=bf'), '#2x@', 'cipher-algo=bf');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 'cipher-algo=blowfish'), '#2x@', 'cipher-algo=blowfish');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 'cipher-algo=twofish'), '#2x@', 'cipher-algo=twofish');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 'cipher-algo=aes192'), '#2x@', 'cipher-algo=aes192');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 'cipher-algo=3des'), '#2x@', 'cipher-algo=3des');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 'cipher-algo=cast5'), '#2x@', 'cipher-algo=cast5');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', ''), '#2x@', '');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 's2k-digest-algo=md5'), '#2x@', 's2k-digest-algo=md5');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 's2k-digest-algo=sha1'), '#2x@', 's2k-digest-algo=sha1');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 's2k-cipher-algo=bf'), '#2x@', 's2k-cipher-algo=bf');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 's2k-cipher-algo=aes'), '#2x@', 's2k-cipher-algo=aes');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 's2k-cipher-algo=aes128'), '#2x@', 's2k-cipher-algo=aes128');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 's2k-cipher-algo=aes192'), '#2x@', 's2k-cipher-algo=aes192');

select pgp_sym_decrypt(pgp_sym_encrypt('1234abcd', '#2x@', 's2k-cipher-algo=aes256'), '#2x@', 's2k-cipher-algo=aes256');
