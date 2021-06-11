-- When we eval a RECORD for the projection, we'll use the slot's type ID info.
-- It's likely that the slot's type is also RECORD; if so, make sure it's been
-- "blessed" (assign_record_type_typmod), so that the Datum can be interpreted
-- later. Because some plan trees may return different slots at different times,
-- we should make sure we do for all of them. See assign_record_type_typmod in
-- ExecEvalWholeRowFast for more details.

1: create table t_a (id int, a int, b text);
1: insert into t_a select i, i, 'a' || i from generate_series(1, 5) as i;
1: create table t_b (id int, a int, b text);
1: insert into t_b select i, i, 'a' || i from generate_series(1, 5) as i;

1: create table t_c (id int);
1: insert into t_c values (1);

1: explain WITH T AS (
	select t_a.* from t_a inner join t_c on t_a.id = t_c.id
UNION ALL
	select t_b.* from t_b inner join t_c on t_b.id = t_c.id)
select ARRAY_TO_JSON(ARRAY_AGG(ROW_TO_JSON(T))) from T;
-- quit the seesion to clear type cache
1q:

-- this will generate a type cache for a RECORD with tupledesc, and the typemod is 0.
1: select ('aaa'::text, 'aaa'::text, 'a'::text);

-- the below query used to fail an assertion since the second subnode for the Append node
-- still return a RECORD type slot, but we forget to assign_record_type_typmod for it,
-- so the typmod is still -1.
-- And when we send the slot tuple to upper slice, tuple remap will raise assertion failure
-- in function TRRemapRecord(). If turn off --cassert, the below query will crash, since it
-- tries to parse the below output RECORD using the above SELECT statement's RECORD tupledesc.
1: WITH T AS (
	select t_a.* from t_a inner join t_c on t_a.id = t_c.id
UNION ALL
	select t_b.* from t_b inner join t_c on t_b.id = t_c.id)
select ARRAY_TO_JSON(ARRAY_AGG(ROW_TO_JSON(T))) from T;

