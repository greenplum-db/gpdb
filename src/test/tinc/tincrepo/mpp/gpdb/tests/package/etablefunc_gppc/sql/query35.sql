-- Create view using ETF function

    CREATE VIEW t1viewetf AS
    SELECT * FROM transform( 
        TABLE(select a,e from t1 
              where a < 10
              order by a
              scatter by a
        )
    );
    -- This should succeed

    -- Describe the created view
    \d+ t1viewetf

    -- directly using transform to create view
    CREATE VIEW t1viewetf AS
    transform( 
        TABLE(select a,e from t1 
              where a < 10
              order by a
              scatter by a
        )
    );
    -- This should fail since ETF is not call via FROM

    -- create view using ETF, where ETF itself is using another view
    create view t1_etf_view as (
        select * from transform( 
            table(select a,e from t1_view order by b scatter randomly) 
    ) ) order by a;
    -- This should succeed
  
    \d+ t1_etf_view

    -- Create temp table (CTAS) using ETF 
    create temp table tmp_t2 as select * from transform( 
        table(select a,e from t1 where a<=10) );
    -- This should succeed

    select * from tmp_t2 order by b;

    drop view t1viewetf;
    drop view t1_etf_view;

