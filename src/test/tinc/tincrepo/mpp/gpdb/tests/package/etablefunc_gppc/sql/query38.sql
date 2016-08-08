-- ETF sub-query calling from CTE, i.e. WITH clause
      WITH first_5 AS (
          SELECT b,a FROM transform( TABLE(
              select a,e from t1 where a <= 5
          )) 
      )
      select * from first_5 order by b;
