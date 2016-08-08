create user mdt_user1 with superuser;
create group mdt_group1 with superuser;

create schema mdt_schema1;
grant create on  schema mdt_schema1 to mdt_user1 with grant option;

create schema mdt_schema2;
grant usage on schema mdt_schema2 to group mdt_group1 with grant option;

create schema mdt_schema3;
grant all privileges on schema mdt_schema3 to public;

create schema mdt_schema4;
grant all on schema mdt_schema4 to public;

drop schema mdt_schema1;
drop schema mdt_schema2;
drop schema mdt_schema3;
drop schema mdt_schema4;
drop user mdt_user1;
drop group mdt_group1;
