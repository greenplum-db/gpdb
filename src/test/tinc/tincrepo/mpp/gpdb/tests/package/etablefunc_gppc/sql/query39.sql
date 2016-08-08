-- ETF sub-query recursive call
      SELECT * FROM transform( TABLE(
          select b,a from transform (TABLE(
              select a,e from t1 where a < 5
          ) )
      ) ) order by a;

      SELECT * FROM transform( TABLE(
          select b,a from transform (TABLE(
              select a,e from t1 where a < 5
              order by d
              scatter by c
          ) )
          order by b
          scatter by a
      ) )
      order by b desc;

      SELECT * FROM transform( TABLE(
      SELECT b,a FROM transform( TABLE(
          select b,a from transform (TABLE(
              select a,e from t1 where a < 5
              order by d
              scatter randomly
          ) )
          order by b
          scatter by a
      ) )
      order by b desc
      scatter by b)) order by a;

