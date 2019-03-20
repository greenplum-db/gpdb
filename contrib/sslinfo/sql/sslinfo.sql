\! echo "Enable SSL in postgresql.conf with master only..."
\! echo "#BEGIN SSLINFO CONF" >> $MASTER_DATA_DIRECTORY/postgresql.conf
\! echo "ssl=on" >> $MASTER_DATA_DIRECTORY/postgresql.conf
\! echo "ssl_ciphers='HIGH:MEDIUM:+3DES:!aNULL'" >> $MASTER_DATA_DIRECTORY/postgresql.conf
\! echo "ssl_cert_file='server.crt'" >> $MASTER_DATA_DIRECTORY/postgresql.conf
\! echo "ssl_key_file='server.key'" >> $MASTER_DATA_DIRECTORY/postgresql.conf
\! echo "ssl_ca_file='root.crt'" >> $MASTER_DATA_DIRECTORY/postgresql.conf
\! echo "#END SSLINFO CONF" >> $MASTER_DATA_DIRECTORY/postgresql.conf

\! echo "preparing CRTs and KEYs"
\! cp -f data/root.crt   $MASTER_DATA_DIRECTORY/
\! cp -f data/server.crt $MASTER_DATA_DIRECTORY/
\! cp -f data/server.key $MASTER_DATA_DIRECTORY/
\! chmod 400 $MASTER_DATA_DIRECTORY/server.key
\! chmod 644 $MASTER_DATA_DIRECTORY/server.crt
\! chmod 644 $MASTER_DATA_DIRECTORY/root.crt
\! mkdir -p ~/.postgresql
\! cp -f data/root.crt         ~/.postgresql/
\! cp -f data/postgresql.crt   ~/.postgresql/
\! cp -f data/postgresql.key   ~/.postgresql/
\! chmod 400 ~/.postgresql/postgresql.key
\! chmod 644 ~/.postgresql/postgresql.crt
\! chmod 644 ~/.postgresql/root.crt

\! gpstop -arf 2>&1  >/dev/null
\! echo "gpstop ret = $?"

\c - - localhost

CREATE EXTENSION sslinfo;

SELECT ssl_is_used();
SELECT ssl_version();
SELECT ssl_cipher();
SELECT ssl_client_cert_present();
SELECT ssl_client_serial();
SELECT ssl_client_dn();
SELECT ssl_issuer_dn();
SELECT ssl_client_dn_field('CN') AS client_dn_CN;
SELECT ssl_client_dn_field('C') AS client_dn_C;
SELECT ssl_client_dn_field('ST') AS client_dn_ST;
SELECT ssl_client_dn_field('L') AS client_dn_L;
SELECT ssl_client_dn_field('O') AS client_dn_O;
SELECT ssl_client_dn_field('OU') AS client_dn_OU;
SELECT ssl_issuer_field('CN') AS issuer_CN;
SELECT ssl_issuer_field('C') AS issuer_C;
SELECT ssl_issuer_field('ST') AS issuer_ST;
SELECT ssl_issuer_field('L') AS issuer_L;
SELECT ssl_issuer_field('O') AS issuer_O;
SELECT ssl_issuer_field('OU') AS issuer_OU;

DROP EXTENSION sslinfo;

\! sed -ri '/#BEGIN SSLINFO CONF/,/#END SSLINFO CONF/d' $MASTER_DATA_DIRECTORY/postgresql.conf
\! gpstop -arf 2>&1  >/dev/null
\! echo "gpstop ret = $?"
