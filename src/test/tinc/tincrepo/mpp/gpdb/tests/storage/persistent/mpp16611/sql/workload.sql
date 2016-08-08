-- start_ignore
drop function if exists f();
-- end_ignore
create or replace function f() returns void as $$
declare begin return; end;
$$ language plpgsql;
set gp_test_system_cache_flush_force = plain;
comment on function f() is $$
@long_comment@
$$;
