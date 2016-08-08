-- 
-- @created 2014-04-14 12:00:00
-- @modified 2014-04-14 12:00:00
-- @tags storage
-- @description AOCO table: Alter data type So that a segfile move happens

Drop table if exists change_col_type;
CREATE table change_col_type(i int,  j int) with (appendonly=true,orientation=column);
 \d change_col_type;
INSERT into change_col_type  select i, i+10 from generate_series(1,1000) i;
ALTER table change_col_type alter column j  type numeric;  
INSERT into change_col_type  select i, i+10 from generate_series(1,1000) i;
 \d change_col_type;
