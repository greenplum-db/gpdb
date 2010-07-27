/* $PostgreSQL: pgsql/contrib/sslinfo/uninstall_sslinfo.sql,v 1.4 2010/07/27 23:43:42 rhaas Exp $ */

-- Adjust this setting to control where the objects get dropped.
SET search_path = public;

DROP FUNCTION ssl_client_serial();
DROP FUNCTION ssl_is_used();
DROP FUNCTION ssl_cipher();
DROP FUNCTION ssl_version();
DROP FUNCTION ssl_client_cert_present();
DROP FUNCTION ssl_client_dn_field(text);
DROP FUNCTION ssl_issuer_field(text);
DROP FUNCTION ssl_client_dn();
DROP FUNCTION ssl_issuer_dn();
