-- calling undefined ETF function

    SELECT * FROM tabfunc_non( TABLE(select * from t1) );

    SELECT * FROM transform( TABLE_non(select * from t1) );
