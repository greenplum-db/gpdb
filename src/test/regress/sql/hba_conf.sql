-- Test ldap
-- backup pg_hba.conf
\! cp $COORDINATOR_DATA_DIRECTORY/pg_hba.conf $COORDINATOR_DATA_DIRECTORY/pg_hba.conf.bak
\! echo 'hostnossl       all     all     10.10.100.100/32        ldap ldapserver="abc.example.com" ldapbasedn="DC=COM" ldapbinddn="OU=Hosting,DC=COM" ldapbindpasswd="ldapbindpasswd111" ldapport=3268 ldaptls=1' >> $COORDINATOR_DATA_DIRECTORY/pg_hba.conf
select type,database,user_name,address,netmask,auth_method,options from pg_hba_file_rules where address = '10.10.100.100';
\! mv $COORDINATOR_DATA_DIRECTORY/pg_hba.conf.bak $COORDINATOR_DATA_DIRECTORY/pg_hba.conf
