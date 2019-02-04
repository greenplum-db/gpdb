--
-- Tests for legacy cdbhash opclasses
--

-- Basic sanity check of all the legacy hash opclasses. Create a table that
-- uses all of them in the distribution key, and insert a value.
set gp_use_legacy_hashops=on;
create table all_legacy_types(
  int2_col int2,
  int4_col int4,
  int8_col int8,
  float4_col float4,
  float8_col float8,
  numeric_col numeric,
  char_col "char",
  bpchar_col char(20),
  text_col text,
  varchar_col varchar(20),
  bytea_col bytea,
  name_col name,
  oid_col oid,
  tid_col tid,
  timestamp_col timestamp,
  timestamptz_col timestamptz,
  date_col date,
  time_col time,
  timetz_col timetz,
  interval_col interval,
  abstime_col abstime,
  tinterval_col tinterval,
  inet_col inet,
  cidr_col cidr,
  macaddr_col macaddr,
  bit_col bit(11),
  varbit_col varbit(20),
  bool_col bool,
  oidvector_col oidvector,
  money_col money,
  --uuid_col uuid,
  complex_col complex
) distributed by (
  int2_col,
  int4_col,
  int8_col,
  float4_col,
  float8_col,
  numeric_col,
  char_col,
  bpchar_col,
  text_col,
  varchar_col,
  bytea_col,
  name_col,
  oid_col,
  tid_col,
  timestamp_col,
  timestamptz_col,
  date_col,
  time_col,
  timetz_col,
  interval_col,
  abstime_col,
  tinterval_col,
  inet_col,
  cidr_col,
  macaddr_col,
  bit_col,
  varbit_col,
  bool_col,
  oidvector_col,
  money_col,
  --uuid_col,
  complex_col
);

-- Verify that all columns are using the legacy hashops
select distkey, distclass from gp_distribution_policy where localoid='all_legacy_types'::regclass;

insert into all_legacy_types values (
  '12345',          -- int2
  '12345678',       -- int4
  '1234567890123',  -- int8
  '1.2345',         -- float4
  '1.23456789',     -- float8
  '1.23456789',     -- numeric
  'x',              -- "char"
  'bpchar foobar',  -- bpchar
  'text foobar',     -- text
  'varchar foobar', -- varchar
  '\xdeadbeef',     -- bytea
  'name foobar',    -- name
  '12345',          -- oid
  '(1,1)',          -- tid
  '2018-11-25 01:23:45+02',   -- timestamp
  '2018-11-25 01:23:45+02',   -- timestamptz
  '2018-11-25',     -- date,
  '01:23:45',       -- time,
  '01:23:45+02',    -- timetz,
  '6 years',        -- interval,
  'Mon May  1 00:30:30 1995', -- abstime,
  '["Feb 15 1990 12:15:03" "2001-09-23 11:12:13"]', -- tinterval,
  '192.168.1.255/25',   -- inet,
  '10.1.2',         -- cidr,
  '08:00:2b:01:02:03',         -- macaddr,
  B'11011000000',   -- bit,
  B'01010101010',   -- varbit,
  'true',        -- bool,
  '1 2 3 4',        -- oidvector,
  '123.45',        -- money,
  --uuid_col         -- uuid,
  '5 + 3i'         -- complex
);

set gp_use_legacy_hashops=off;


--
-- Test joins between tables using a mix of default and legacy opclasses.
--
create table legacy_int (id int4) distributed by (id cdbhash_int4_ops);
insert into legacy_int values (1), (2), (3);

create table modern_int (id int4) distributed by (id);
insert into modern_int values (2), (3), (4);

create table modern_text (t text) distributed by (t);
insert into modern_text values ('foo'), ('1');

-- Only legacy opclasses used. ORCA can deal with this
explain (costs off) select * from legacy_int a inner join legacy_int b on a.id = b.id;
select * from legacy_int a inner join legacy_int b on a.id = b.id;

-- A mixture of legacy and modern opclasses. Needs a Redistribute motion. ORCA
-- can't handle this, and falls back.
explain (costs off) select * from legacy_int a inner join modern_int b on a.id = b.id;
select * from legacy_int a inner join modern_int b on a.id = b.id;

-- for the sake of completeness, modern opclasses only. No problem for ORCA.
explain (costs off) select * from modern_int a inner join modern_int b on a.id = b.id;
select * from modern_int a inner join modern_int b on a.id = b.id;

-- In principle, ORCA would create a correct plan for this, because even though it
-- mixes the legacy and other opclasses, they're not used in join conditions. But
-- the ORCA translator code is conservative, and falls back if there are any mixed
-- use, even if it would be safe.
explain (costs off) select * from legacy_int a inner join modern_text b on a.id::text = b.t;
select * from legacy_int a inner join modern_text b on a.id::text = b.t;


-- Also test with a domain over a base type, and enums.
create domain intdom as integer;
create table legacy_domain_over_int(id intdom) distributed by(id cdbhash_int4_ops);
insert into legacy_domain_over_int values (1), (2), (3);

explain (costs off) select * from legacy_domain_over_int a inner join legacy_domain_over_int b on a.id = b.id;
explain (costs off) select * from legacy_int a inner join legacy_domain_over_int b on a.id = b.id;
explain (costs off) select * from modern_int a inner join legacy_domain_over_int b on a.id = b.id;

create type colors as enum ('red', 'green', 'blue');
create table legacy_enum(color colors) distributed by(color cdbhash_enum_ops);
insert into legacy_enum values ('red'), ('green'), ('blue');

explain (costs off) select * from legacy_enum a inner join legacy_enum b on a.color = b.color;
select * from legacy_enum a inner join legacy_enum b on a.color = b.color;
