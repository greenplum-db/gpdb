create user mdt_user1 with superuser;
create user mdt_user2 with superuser;
create user mdt_user3 with superuser;
create user mdt_user4 with superuser;

grant mdt_user1 to mdt_user2 with admin option;
revoke admin option for mdt_user1 from mdt_user2 cascade;

grant mdt_user3 to mdt_user4 with admin option;
revoke admin option for mdt_user3 from mdt_user4 restrict;

drop user mdt_user1;
drop user mdt_user2;
drop user mdt_user3;
drop user mdt_user4;

