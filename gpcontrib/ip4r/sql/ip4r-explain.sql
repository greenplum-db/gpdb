-- predicates and indexing

set enable_seqscan = "off";

create table ipranges_exp (r iprange, r4 ip4r, r6 ip6r) distributed by (r);

insert into ipranges_exp
select r, null, r
  from (select ip6r(regexp_replace(ls, E'(....(?!$))', E'\\1:', 'g')::ip6,
                    regexp_replace(substring(ls for n+1) || substring(us from n+2),
                                   E'(....(?!$))', E'\\1:', 'g')::ip6) as r
          from (select md5(i || ' lower 1') as ls,
                       md5(i || ' upper 1') as us,
                       (i % 11) + (i/11 % 11) + (i/121 % 11) as n
                  from generate_series(1,13310) i) s1) s2;

insert into ipranges_exp
select r, r, null
  from (select ip4r(ip4 '0.0.0.0' + ((la & '::ffff:ffff') - ip6 '::'),
                    ip4 '0.0.0.0' + ((( (la & ip6_netmask(127-n)) | (ua & ~ip6_netmask(127-n)) ) & '::ffff:ffff') - ip6 '::')) as r
          from (select regexp_replace(md5(i || ' lower 2'), E'(....(?!$))', E'\\1:', 'g')::ip6 as la,
                       regexp_replace(md5(i || ' upper 2'), E'(....(?!$))', E'\\1:', 'g')::ip6 as ua,
                       (i % 11) + (i/11 % 11) + (i/121 % 11) as n
                  from generate_series(1,1331) i) s1) s2;

insert into ipranges_exp
select r, null, r
  from (select n::ip6 / 68 as r
          from (select ((8192 + i/256)::numeric * (2::numeric ^ 112)
                       + (131072 + (i % 256))::numeric * (2::numeric ^ 60)) as n
                  from generate_series(0,4095) i) s1) s2;

insert into ipranges_exp
select r, r, null
  from (select n / 28 as r
          from (select ip4 '172.16.0.0' + (i * 256) as n
                  from generate_series(0,4095) i) s1) s2;

insert into ipranges_exp
select r, null, r
  from (select n::ip6 / 48 as r
          from (select ((8192 + i/256)::numeric * (2::numeric ^ 112)
                       + (i % 256)::numeric * (2::numeric ^ 84)) as n
                  from generate_series(0,4095) i) s1) s2;

insert into ipranges_exp
select r, r, null
  from (select n / 16 as r
          from (select ip4 '128.0.0.0' + (i * 65536) as n
                  from generate_series(0,4095) i) s1) s2;

insert into ipranges_exp values ('-',null,null);

create table ipaddrs_exp (a ipaddress, a4 ip4, a6 ip6) distributed by (a);

insert into ipaddrs_exp
select a, null, a
  from (select regexp_replace(md5(i || ' address 1'), E'(....(?!$))', E'\\1:', 'g')::ip6 as a
          from generate_series(1,256) i) s1;

insert into ipaddrs_exp
select a, a, null
  from (select ip4 '0.0.0.0' + ((regexp_replace(md5(i || ' address 1'), E'(....(?!$))', E'\\1:', 'g')::ip6 & '::ffff:ffff') - '::') as a
          from generate_series(1,16) i) s1;

explain select * from ipranges_exp where r >>= '5555::' order by r;
explain select * from ipranges_exp where r <<= '5555::/16' order by r;
explain select * from ipranges_exp where r && '5555::/16' order by r;
explain select * from ipranges_exp where r6 >>= '5555::' order by r6;
explain select * from ipranges_exp where r6 <<= '5555::/16' order by r6;
explain select * from ipranges_exp where r6 && '5555::/16' order by r6;
explain select * from ipranges_exp where r >>= '172.16.2.0' order by r;
explain select * from ipranges_exp where r <<= '10.0.0.0/12' order by r;
explain select * from ipranges_exp where r && '10.128.0.0/12' order by r;
explain select * from ipranges_exp where r4 >>= '172.16.2.0' order by r4;
explain select * from ipranges_exp where r4 <<= '10.0.0.0/12' order by r4;
explain select * from ipranges_exp where r4 && '10.128.0.0/12' order by r4;

explain select * from ipranges_exp where r >>= '2001:0:0:2000:a123::' order by r;
explain select * from ipranges_exp where r >>= '2001:0:0:2000::' order by r;
explain select * from ipranges_exp where r >>= '2001:0:0:2000::/68' order by r;
explain select * from ipranges_exp where r >> '2001:0:0:2000::/68' order by r;

explain select * from ipranges_exp where r6 >>= '2001:0:0:2000:a123::' order by r6;
explain select * from ipranges_exp where r6 >>= '2001:0:0:2000::' order by r6;
explain select * from ipranges_exp where r6 >>= '2001:0:0:2000::/68' order by r6;
explain select * from ipranges_exp where r6 >> '2001:0:0:2000::/68' order by r6;

explain select * from ipranges_exp where r4 >>= '172.16.2.0/28' order by r4;
explain select * from ipranges_exp where r4 >> '172.16.2.0/28' order by r4;

explain select * from ipaddrs_exp where a between '8.0.0.0' and '15.0.0.0' order by a;
explain select * from ipaddrs_exp where a4 between '8.0.0.0' and '15.0.0.0' order by a4;

create index ipranges_exp_r on ipranges_exp using gist (r);
create index ipranges_exp_r4 on ipranges_exp using gist (r4);
create index ipranges_exp_r6 on ipranges_exp using gist (r6);
create index ipaddrs_exp_a on ipaddrs_exp (a);
create index ipaddrs_exp_a4 on ipaddrs_exp (a4);
create index ipaddrs_exp_a6 on ipaddrs_exp (a6);

analyze ipranges_exp;
analyze ipaddrs_exp;

explain select * from ipranges_exp where r >>= '5555::' order by r;
explain select * from ipranges_exp where r <<= '5555::/16' order by r;
explain select * from ipranges_exp where r && '5555::/16' order by r;
explain select * from ipranges_exp where r6 >>= '5555::' order by r6;
explain select * from ipranges_exp where r6 <<= '5555::/16' order by r6;
explain select * from ipranges_exp where r6 && '5555::/16' order by r6;
explain select * from ipranges_exp where r >>= '172.16.2.0' order by r;
explain select * from ipranges_exp where r <<= '10.0.0.0/12' order by r;
explain select * from ipranges_exp where r && '10.128.0.0/12' order by r;
explain select * from ipranges_exp where r4 >>= '172.16.2.0' order by r4;
explain select * from ipranges_exp where r4 <<= '10.0.0.0/12' order by r4;
explain select * from ipranges_exp where r4 && '10.128.0.0/12' order by r4;

explain select * from ipranges_exp where r >>= '2001:0:0:2000:a123::' order by r;
explain select * from ipranges_exp where r >>= '2001:0:0:2000::' order by r;
explain select * from ipranges_exp where r >>= '2001:0:0:2000::/68' order by r;
explain select * from ipranges_exp where r >> '2001:0:0:2000::/68' order by r;

explain select * from ipranges_exp where r6 >>= '2001:0:0:2000:a123::' order by r6;
explain select * from ipranges_exp where r6 >>= '2001:0:0:2000::' order by r6;
explain select * from ipranges_exp where r6 >>= '2001:0:0:2000::/68' order by r6;
explain select * from ipranges_exp where r6 >> '2001:0:0:2000::/68' order by r6;

explain select * from ipranges_exp where r4 >>= '172.16.2.0/28' order by r4;
explain select * from ipranges_exp where r4 >> '172.16.2.0/28' order by r4;

explain select * from ipaddrs_exp where a between '8.0.0.0' and '15.0.0.0' order by a;
explain select * from ipaddrs_exp where a4 between '8.0.0.0' and '15.0.0.0' order by a4;

explain select * from ipaddrs_exp a join ipranges_exp r on (r.r >>= a.a) order by a,r;
explain select * from ipaddrs_exp a join ipranges_exp r on (r.r4 >>= a.a4) order by a4,r4;
explain select * from ipaddrs_exp a join ipranges_exp r on (r.r6 >>= a.a6) order by a6,r6;

-- index-only, on versions that support it:

vacuum ipranges_exp;

explain select r from ipranges_exp where r >>= '5555::' order by r;
explain select r6 from ipranges_exp where r6 >>= '5555::' order by r6;
explain select r4 from ipranges_exp where r4 >>= '172.16.2.0' order by r4;
