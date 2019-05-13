-- start_matchignore
-- m/waiting for server to shut down.* done/
-- m/waiting for server to start.* done/
-- end_matchignore
-- Given no superusers exist with a null rolvaliduntil value
set allow_system_table_mods to dml;
create table stored_superusers (role_name text, role_valid_until timestamp);
insert into stored_superusers (role_name, role_valid_until) select rolname, rolvaliduntil from pg_authid where rolsuper = true;
update pg_authid set rolvaliduntil = 'infinity' where rolsuper = true;

-- And a non-superuser can log in
create user testinguser with nocreatedb nosuperuser nocreaterole;
create database testinguser;
\! echo "local testinguser testinguser trust" >> $MASTER_DATA_DIRECTORY/pg_hba.conf;

-- When the master server restarts
\! pg_ctl -D $MASTER_DATA_DIRECTORY restart -w -m fast;

-- And a non-superuser happens to be the first user to connect
-- Then the connection should succeed
\! psql -U testinguser -c 'select 1;';

-- cleanup
\c
drop database testinguser;
drop user testinguser;
set allow_system_table_mods to dml;
update pg_authid set rolvaliduntil = stored_superusers.role_valid_until from stored_superusers where rolname=stored_superusers.role_name;
drop table stored_superusers;

\! sed -i 's/local testinguser testinguser trust//' $MASTER_DATA_DIRECTORY/pg_hba.conf
\! pg_ctl -D $MASTER_DATA_DIRECTORY restart -w -m fast;
