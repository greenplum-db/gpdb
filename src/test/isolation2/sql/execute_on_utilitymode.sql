--
-- Test using functions with EXECUTE ON options in utility mode.
--

-- First, create test functions with different EXECUTE ON options

create function srf_on_coordinator () returns setof text as $$
begin	/* in func */
  return next 'foo ' || current_setting('gp_contentid');	/* in func */
  return next 'bar ' || current_setting('gp_contentid');	/* in func */
end;	/* in func */
$$ language plpgsql EXECUTE ON COORDINATOR;

create function srf_on_all_segments () returns setof text as $$
begin	/* in func */
  return next 'foo ' || current_setting('gp_contentid');	/* in func */
  return next 'bar ' || current_setting('gp_contentid');	/* in func */
end;	/* in func */
$$ language plpgsql EXECUTE ON ALL SEGMENTS;

create function srf_on_any () returns setof text as $$
begin	/* in func */
  return next 'foo ' || current_setting('gp_contentid');	/* in func */
  return next 'bar ' || current_setting('gp_contentid');	/* in func */
end;	/* in func */
$$ language plpgsql EXECUTE ON ANY IMMUTABLE;

create function srf_on_initplan () returns setof text as $$
begin	/* in func */
  return next 'foo ' || current_setting('gp_contentid');	/* in func */
  return next 'bar ' || current_setting('gp_contentid');	/* in func */
end;	/* in func */
$$ language plpgsql EXECUTE ON INITPLAN;

-- Now try executing them in utility mode, in the coordinator node and on a
-- segment. The expected behavior is that the function runs on the node
-- we're connected to, ignoring the EXECUTE ON directives.
--
-- Join with a table, to give the planner something more exciting to do
-- than just create the FunctionScan plan.
create table fewrows (t text) distributed by (t);
insert into fewrows select g from generate_series(1, 10) g;

-1U: select * from srf_on_coordinator()       as srf (x) left join fewrows on x = t;
-1U: select * from srf_on_all_segments() as srf (x) left join fewrows on x = t;
-1U: select * from srf_on_any()          as srf (x) left join fewrows on x = t;
-1U: select * from srf_on_initplan()     as srf (x) left join fewrows on x = t;

1U: select * from srf_on_coordinator(),       fewrows;
1U: select * from srf_on_all_segments(), fewrows;
1U: select * from srf_on_any(),          fewrows;
1U: select * from srf_on_initplan(),     fewrows;
