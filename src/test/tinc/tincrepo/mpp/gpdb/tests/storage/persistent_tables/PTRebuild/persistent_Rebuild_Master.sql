-- Rebuild Persistent Object on Master --
select * from gp_persistent_reset_all();
select * from gp_persistent_build_all(false);
