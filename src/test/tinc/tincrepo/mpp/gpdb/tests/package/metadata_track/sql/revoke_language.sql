create user mdt_user1 with superuser;
create group mdt_group1 with superuser;

create language plperl;
grant usage on language plperl to mdt_user1 with grant option;
revoke grant option for usage on language plperl from mdt_user1;

grant usage on language plpgsql to group mdt_group1 with grant option;
REVOKE  usage on language plpgsql from group mdt_group1 cascade;

grant all privileges on language sql to public;
revoke all privileges on language sql from public restrict; 

drop language plperl;
drop user mdt_user1;
drop group mdt_group1;
