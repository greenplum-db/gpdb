--
-- CREATE_AGGREGATE
--

-- all functions CREATEd
CREATE AGGREGATE newavg (
   sfunc = int4_avg_accum, basetype = int4, stype = bytea, 
   finalfunc = int8_avg,
   initcond1 = '{0}'
);

-- test comments
COMMENT ON AGGREGATE newavg_wrong (int4) IS 'an agg comment';
COMMENT ON AGGREGATE newavg (int4) IS 'an agg comment';
COMMENT ON AGGREGATE newavg (int4) IS NULL;

-- without finalfunc; test obsolete spellings 'sfunc1' etc
CREATE AGGREGATE newsum (
   sfunc1 = int4pl, basetype = int4, stype1 = int4, 
   initcond1 = '0'
);

-- zero-argument aggregate
CREATE AGGREGATE newcnt (*) (
   sfunc = int8inc, stype = int8,
   initcond = '0'
);

-- old-style spelling of same
CREATE AGGREGATE oldcnt (
   sfunc = int8inc, basetype = 'ANY', stype = int8,
   initcond = '0'
);

-- aggregate that only cares about null/nonnull input
CREATE AGGREGATE newcnt ("any") (
   sfunc = int8inc_any, stype = int8,
   initcond = '0'
);

COMMENT ON AGGREGATE nosuchagg (*) IS 'should fail';
COMMENT ON AGGREGATE newcnt (*) IS 'an agg(*) comment';
COMMENT ON AGGREGATE newcnt ("any") IS 'an agg(any) comment';

-- multi-argument aggregate
create function sum3(int8,int8,int8) returns int8 as
'select $1 + $2 + $3' language sql CONTAINS SQL strict immutable;

create aggregate sum2(int8,int8) (
   sfunc = sum3, stype = int8,
   initcond = '0'
);

-- multi-argument aggregates sensitive to distinct/order, strict/nonstrict
create type aggtype as (a integer, b integer, c text);

create function aggf_trans(aggtype[],integer,integer,text) returns aggtype[]
as 'select array_append($1,ROW($2,$3,$4)::aggtype)'
language sql strict immutable;

create function aggfns_trans(aggtype[],integer,integer,text) returns aggtype[]
as 'select array_append($1,ROW($2,$3,$4)::aggtype)'
language sql immutable;

create aggregate aggfstr(integer,integer,text) (
   sfunc = aggf_trans, stype = aggtype[],
   initcond = '{}'
);

create aggregate aggfns(integer,integer,text) (
   sfunc = aggfns_trans, stype = aggtype[],
   initcond = '{}'
);

-- variadic aggregate
create function least_accum(anyelement, variadic anyarray)
returns anyelement language sql as
  'select least($1, min($2[i])) from generate_subscripts($2,1) g(i)';

create aggregate least_agg(variadic items anyarray) (
  stype = anyelement, sfunc = least_accum
);

-- test ordered-set aggs using built-in support functions
create aggregate my_percentile_disc(float8 ORDER BY anyelement) (
  stype = internal,
  sfunc = ordered_set_transition,
  finalfunc = percentile_disc_final,
  finalfunc_extra = true
);

create aggregate my_rank(VARIADIC "any" ORDER BY VARIADIC "any") (
  stype = internal,
  sfunc = ordered_set_transition_multi,
  finalfunc = rank_final,
  finalfunc_extra = true,
  hypothetical
);

alter aggregate my_percentile_disc(float8 ORDER BY anyelement)
  rename to test_percentile_disc;
alter aggregate my_rank(VARIADIC "any" ORDER BY VARIADIC "any")
  rename to test_rank;

\da test_*


-- Negative test: "ordered aggregate prefunc is not supported"
create ordered aggregate should_error(integer,integer,text) (
   stype = aggtype[],
   sfunc = aggfns_trans, 
   prefunc = array_cat,
   initcond = '{}'
);

-- MPP-2863: ensure that aggregate declarations with an initial value == ''
-- do not get converted to an initial value == NULL
create function str_concat(t1 text, t2 text) returns text as
$$
    select $1 || $2;
$$ language sql CONTAINS SQL;

CREATE AGGREGATE string_concat (sfunc = str_concat, prefunc=str_concat, basetype = 'text', stype = text,initcond = '');

create table aggtest2(i int, t text) DISTRIBUTED BY (i);
insert into aggtest2 values(1, 'hello');
insert into aggtest2 values(2, 'hello');
select string_concat(t) from aggtest2;
select string_concat(t) from (select * from aggtest2 limit 2000) tmp;
drop table aggtest2;
drop aggregate string_concat(text);
drop function str_concat(text, text);
