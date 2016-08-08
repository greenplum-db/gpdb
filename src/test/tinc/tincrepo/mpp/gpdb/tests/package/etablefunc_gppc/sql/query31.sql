-- Negative: ETF function call missing TABLE keyword,
-- this effectively make the sub-query a value expression

     SELECT * FROM tabfunc( (select count(*) from t1) );

     SELECT * FROM tabfunc( (select id, value from t1) );
