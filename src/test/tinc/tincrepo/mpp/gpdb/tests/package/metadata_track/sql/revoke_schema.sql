create user mdt_user1 with superuser;
create group mdt_group1 with superuser;

create schema mdt_schema1;
grant create on  schema mdt_schema1 to mdt_user1 with grant option;
revoke grant option for create on schema mdt_schema1 from mdt_user1 cascade;

create schema mdt_schema2;
grant usage on schema mdt_schema2 to group mdt_group1 with grant option;
revoke usage on schema mdt_schema2 from group mdt_group1 restrict;

create schema mdt_schema3;
grant all privileges on schema mdt_schema3 to public;
revoke all privileges on schema mdt_schema3 from public;

create schema mdt_schema4;
grant all on schema mdt_schema4 to public;
revoke all on schema mdt_schema4 from public;

drop schema mdt_schema1;
drop schema mdt_schema2;
drop schema mdt_schema3;
drop schema mdt_schema4;
drop user mdt_user1;
drop group mdt_group1;
