-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--start_ignore
drop table if exists col_storage_params_table;
--end_ignore
CREATE TABLE col_storage_params_table (
    o_orderkey bigint,
    o_custkey bigint,
    o_orderstatus character(1),
    o_totalprice numeric(15,2),
    o_orderdate date,
    o_orderpriority character(15),
    o_clerk character(15),
    o_shippriority bigint,
    o_comment character varying(79),
    l_orderkey bigint,
    l_partkey bigint,
    l_suppkey bigint,
    l_linenumber bigint,
    l_quantity numeric(15,2),
    l_extendedprice numeric(15,2),
    l_discount numeric(15,2),
    l_tax numeric(15,2),
    l_returnflag character(1),
    l_linestatus character(1),
    l_shipdate date,
    l_commitdate date,
    l_receiptdate date,
    l_shipinstruct character(25),
    l_shipmode character(10),
    l_comment character varying(44)
)
WITH (appendonly=true, compresslevel=9, orientation=column) distributed by (l_orderkey ,l_linenumber);

CREATE OR REPLACE FUNCTION array_to_rows(myarray ANYARRAY) RETURNS SETOF
ANYELEMENT AS $$
  BEGIN
    FOR j IN 1..ARRAY_UPPER(myarray,1) LOOP
      RETURN NEXT myarray[j];
    END LOOP;
    RETURN;
  END;
$$ LANGUAGE 'plpgsql';

select
  n.nspname,
  c.relname,
  a.attnum,
  array_to_rows(a.attoptions)
from
  pg_attribute_encoding a,
  pg_class c,
  pg_namespace n
where a.attrelid = c.oid
  and c.relnamespace = n.oid
  and c.relname = 'col_storage_params_table'
order by 1,2,3,4;
