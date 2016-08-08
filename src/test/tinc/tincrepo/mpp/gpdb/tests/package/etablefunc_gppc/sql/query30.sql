-- Negative: using SCATTER outside of ETF sub-query

     SELECT * FROM tabfunc( TABLE(select * from t1)
         SCATTER BY a
     );
     -- ERROR:  syntax error at or near "scatter"

     SELECT * FROM tabfunc( TABLE(select * from t1) )
     SCATTER BY a;
     -- ERROR:  syntax error at or near "scatter"

     -- using ORDER BY outside of sub-query
     SELECT * FROM tabfunc( TABLE(select * from t1)
         ORDER BY a
     );
     -- ERROR:  ORDER BY specified, but transform is not an ordered aggregate function
