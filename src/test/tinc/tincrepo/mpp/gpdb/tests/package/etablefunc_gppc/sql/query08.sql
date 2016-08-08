-- Missing SETOF keyword in RETURNS
-- Note this is stll valid.
-- The result is one row of record is returned per each segment.
-- For a cluster of 2 segments, total 2 rows will be returned.
DROP FUNCTION IF EXISTS transform_outtable(anytable);
CREATE OR REPLACE FUNCTION transform_outtable(a anytable)
RETURNS TABLE (a text, b int)
AS '$libdir/tabfunc_gppc_demo', 'mytransform'
LANGUAGE C;

select * from transform_outtable(TABLE(select * from intable)) order by b;

drop function if exists transform_outtable(anytable);

-- Missing TABLE keyword in RETURNS
CREATE OR REPLACE FUNCTION transform(a anytable)
RETURNS (a text, b int)
AS '$libdir/tabfunc_gppc_demo', 'mytransform'
LANGUAGE C;

