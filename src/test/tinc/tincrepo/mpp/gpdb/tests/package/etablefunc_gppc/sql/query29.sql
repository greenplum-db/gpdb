-- Negative: Some invalid usages of ETF
-- All following queries should fail

    -- Using **DISTRIBUTED** keyword in sub-query
    SELECT * FROM tabfunc( TABLE(select * from t1 
        DISTRIBUTED RANDOMLY
    ) );
    -- ERROR:  syntax error at or near "distributed"

    -- Using **PARTITION** keyword in sub-query
    SELECT * FROM tabfunc( TABLE(select * from t1 
        PARTITION BY a
    ) );
    -- ERROR:  syntax error at or near "partition"

    -- Sub-query ending with semi-colon ';'
    SELECT * FROM tabfunc( TABLE(select * from t1 
        SCATTER RANDOMLY ;
    ) );
    -- ERROR:  syntax error at or near ";"

    -- source table does not exist
    SELECT * FROM tabfunc( TABLE(select * from non_exist) );
    -- ERROR:  relation "non_exist" does not exist

    -- sub-query is not a select query
    SELECT * FROM tabfunc( TABLE(
        update t1 set e='test_new' where a=200
    ) );
    -- ERROR:  syntax error at or near "update"

    -- using multiple TABLE keyword
    SELECT * FROM tabfunc( TABLE TABLE(select * from t1) );
    -- ERROR:  syntax error at or near "TABLE"

    SELECT * FROM tabfunc( TABLE(select a from t1) 
                           TABLE(select b from t1) );
    -- ERROR:  syntax error at or near "TABLE"

    -- using multiple SCATTER keyword
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATTER BY a
                           SCATTER RANDOMLY) );
    -- ERROR:  syntax error at or near "scatter"

    -- Using scatter before order by
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATTER BY a
                           ORDER BY a) );
    -- ERROR:  syntax error at or near "order"

    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATTER RANDOMLY
                           ORDER BY a) );
    -- ERROR:  syntax error at or near "order"

    -- using multiple ORDER keyword
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           ORDER BY a
                           ORDER BY b) );
    -- ERROR:  syntax error at or near "order"

    SELECT * FROM tabfunc( TABLE(select * from t1 
                           ORDER BY a
                           SCATTER RANDOMLY
                           ORDER BY b) );
    -- ERROR:  syntax error at or near "order"

    -- using incorrect keyword **SCATER**, **SCATTERED** instead of SCATTER
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATER RANDOMLY) );
    -- ERROR:  syntax error at or near "scater"

    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATTERED BY a) );
    -- ERROR:  syntax error at or near "scattered"

    -- using incorrect keyword **ORDERED** instead of ORDER
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           ORDERED BY a) );
    -- ERROR:  syntax error at or near "by"

    -- using incorrect parentheses for scatter by 
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           SCATTER BY (a,b)) );
    -- ERROR:  no equality operator for typid 2249 (cdbmutate.c:1177)

    -- using incorrect parentheses for order by 
    SELECT * FROM tabfunc( TABLE(select * from t1 
                           ORDER BY (a,b)) );
    -- ERROR:  could not identify an ordering operator for type record
