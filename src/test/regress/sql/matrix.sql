create table dtype
(tint2     int2[],
 tint4     int4[],
 tint8     int8[],
 tfloat4   float4[],
 tfloat8   float8[],
 tnumeric  numeric[]) 
Distributed randomly;

insert into dtype values(
  array[1,   2,   3],
  array[1,   2,   3],
  array[1,   2,   3],
  array[1.1, 2.2, 3.3],
  array[1.1, 2.2, 3.3],
  array[1.1, 2.2, 3.3]
);
insert into dtype values(
  array[5,   4,   3],
  array[5,   4,   3],
  array[5,   4,   3],
  array[5.1, 4.2, 3.3],
  array[5.1, 4.2, 3.3],
  array[5.1, 4.2, 3.3]
);

select sum(tint2)    from dtype;
select sum(tint4)    from dtype;
select sum(tint8)    from dtype;
select sum(tfloat4)  from dtype;
select sum(tfloat8)  from dtype;
select sum(tnumeric) from dtype;

-- should be able to handle null values during sumation
insert into dtype values(null, null, null, null, null, null);
select sum(tint2)    from dtype;
select sum(tint4)    from dtype;
select sum(tint8)    from dtype;
select sum(tfloat4)  from dtype;
select sum(tfloat8)  from dtype;
select sum(tnumeric) from dtype;

-- these should fail, but tell us what the return type of sum() was
select sum(tint2)::boolean[]     from dtype;  -- bigint[]
select sum(tint4)::boolean[]     from dtype;  -- bigint[]
select sum(tint8)::boolean[]     from dtype;  -- bigint[]
select sum(tfloat4)::boolean[]   from dtype;  -- float8[]
select sum(tfloat8)::boolean[]   from dtype;  -- float8[]
select sum(tnumeric)::boolean[]  from dtype;  -- float8[]

-- What would normal sum do?
select sum(tint2[1])::boolean     from dtype;  -- bigint
select sum(tint4[1])::boolean     from dtype;  -- bigint
select sum(tint8[1])::boolean     from dtype;  -- numeric  (bigint above)
select sum(tfloat4[1])::boolean   from dtype;  -- float4   (float8 above)
select sum(tfloat8[1])::boolean   from dtype;  -- float8
select sum(tnumeric[1])::boolean  from dtype;  -- numeric

