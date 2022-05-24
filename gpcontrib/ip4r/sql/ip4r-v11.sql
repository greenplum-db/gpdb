-- Tests for pg11+

-- RANGE support

select a4,
       array_agg(a4) over (order by a4 range between 268435456 preceding and 268435456 following)
  from ipaddrs
 where a4 is not null;

select a4,
       array_agg(a4) over (order by a4 range between ip4 '16.0.0.0' preceding and ip4 '16.0.0.0' following)
  from ipaddrs
 where a4 is not null;

select a4,
       array_agg(a4) over (order by a4 range between -3 preceding and -3 following)
  from ipaddrs
 where a4 is not null;

select a6,
       array_agg(a6) over (order by a6 range between -10 preceding and -10 following)
  from ipaddrs
 where a6 is not null;

select a6,
       array_agg(a6) over (order by a6 range between ip6 '0010::' preceding and ip6 '0010::' following)
  from ipaddrs
 where a6 is not null;

-- errors
select a4,
       array_agg(a4) over (order by a4 range between -33 preceding and -33 following)
  from ipaddrs
 where a4 is not null;

select a6,
       array_agg(a6) over (order by a6 range between -129 preceding and -129 following)
  from ipaddrs
 where a6 is not null;

-- end
