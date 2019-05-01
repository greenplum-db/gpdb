-- Given no superusers exist with a null rolvaliduntil value
set allow_system_table_mods to dml;
create table stored_superusers (role_name text, role_valid_until timestamp);
insert into stored_superusers (role_name, role_valid_until) select rolname, rolvaliduntil from pg_authid where rolsuper = true;
update pg_authid set rolvaliduntil = 'infinity' where rolsuper = true;

-- And a non-superuser can log in
\! createuser --no-createdb --no-superuser --no-createrole testinguser;
\! createdb testinguser;
\! echo "local testinguser testinguser trust" >> $MASTER_DATA_DIRECTORY/pg_hba.conf;

-- When the master server restarts
\! pg_ctl -D $MASTER_DATA_DIRECTORY restart -w -m fast;

-- And a non-superuser happens to be the first user to connect
-- Then the connection should succeed
\! psql -U testinguser -c 'select 1;';

-- cleanup
\! dropdb testinguser;
\! dropuser testinguser;
\! sed -i 's/local testinguser testinguser trust//' $MASTER_DATA_DIRECTORY/pg_hba.conf
\! pg_ctl -D $MASTER_DATA_DIRECTORY restart -w -m fast;
\! PGOPTIONS='-c allow_system_table_mods=dml' psql regression -c 'update pg_authid set rolvaliduntil = stored_superusers.role_valid_until from stored_superusers where rolname=stored_superusers.role_name;'
\! psql regression -c 'drop table stored_superusers;'
