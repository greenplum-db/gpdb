-- Rebuild Persistent Object on Segment --
select * from gp_persistent_reset_all();
select * from gp_persistent_build_all(true);
