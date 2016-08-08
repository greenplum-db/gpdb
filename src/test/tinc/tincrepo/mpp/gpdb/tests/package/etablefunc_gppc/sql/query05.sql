-- Create ETF using return syntax: TABLE LIKE table, however the "liked" table is non-existing
    DROP FUNCTION IF EXISTS transform(anytable);
    CREATE OR REPLACE FUNCTION transform (a anytable)
      RETURNS TABLE (LIKE outtable_nonexist)
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

-- Create ETF using return syntax: TABLE LIKE table
    CREATE OR REPLACE FUNCTION transform(a anytable)
      RETURNS TABLE (LIKE outtable)
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

-- Verify ETF can be called successfully
select * from transform(TABLE(select * from intable where id<3));

-- Add a new column to output table, then drop the new added column.
alter table outtable add column newcol int;
alter table outtable drop column newcol;
\d outtable;

-- Calling ETF again, get ERROR: invalid output tuple
-- MPP-14231
select * from transform(TABLE(select * from intable where id<3));

-- Recreate outtable
DROP TABLE IF EXISTS outtable cascade;
CREATE TABLE outtable(a text, b int) distributed randomly;

-- Recreate transform function
CREATE OR REPLACE FUNCTION transform (a anytable)
      RETURNS TABLE (a text, b int)
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;
