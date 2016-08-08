create user mdt_user1 with superuser;
create user mdt_user2 with superuser;
grant mdt_user1 to mdt_user2 with admin option;

drop user mdt_user1;
drop user mdt_user2;
