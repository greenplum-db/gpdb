-- @author onosen
-- @created 2015-01-08 18:00:00
-- @tags ORCA bfv
-- @gpopt 1.522
-- @description bfv for MPP-22263
-- @skip MPP-22263

--start_ignore
explain
select * from mpp22263, (values(147, 'RFAAAA'), (931, 'VJAAAA')) as v (i, j)
    WHERE mpp22263.unique1 = v.i and mpp22263.stringu1 = v.j;
--end_ignore

\!grep 'Filter: mpp22263.stringu1::text = "outer".column2' %MYD%/output_bfv_other/mpp22263_orca.out
