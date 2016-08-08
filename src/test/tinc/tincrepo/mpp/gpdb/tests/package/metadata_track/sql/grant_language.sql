create user mdt_user1 with superuser;
create group mdt_group1 with superuser;

-- start_ignore
create language plperl;
-- end_ignore
grant usage on language plperl to mdt_user1 with grant option;

grant usage on language plpgsql to group mdt_group1 with grant option;

grant all privileges on language sql to public;

drop language plperl;
drop user mdt_user1;
REVOKE  usage on language plpgsql from group mdt_group1 cascade;
drop group mdt_group1;
