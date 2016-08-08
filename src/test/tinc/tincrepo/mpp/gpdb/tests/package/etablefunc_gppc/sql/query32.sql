-- Negative: ETF function call missing or extra parentheses
    SELECT * FROM tabfunc( TABLE select * from t1 );
    -- ERROR: syntax error at or near "SELECT"

    SELECT * FROM tabfunc TABLE (select * from t1);
    -- ERROR:  syntax error at or near "TABLE"

    SELECT * FROM tabfunc TABLE select * from t1;
    -- ERROR:  syntax error at or near "TABLE"

    SELECT * FROM tabfunc (TABLE) select * from t1;
    -- ERROR:  syntax error at or near ")"

    -- cases of extra parentheses
    SELECT * FROM transform( TABLE( SELECT * FROM intable) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE(( SELECT * FROM intable)) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE((( SELECT * FROM intable))) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE(( SELECT * FROM intable ORDER BY ID)) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE((( SELECT * FROM intable ORDER BY ID))) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE( SELECT * FROM intable SCATTER BY ID) ) ORDER BY b; -- ok
    SELECT * FROM transform( TABLE(( SELECT * FROM intable SCATTER BY ID)) ); -- ERROR:  syntax error at or near "SCATTER"
    SELECT * FROM transform( TABLE(( SELECT * FROM intable ORDER BY ID SCATTER BY ID)) ); -- ERROR:  syntax error at or near "SCATTER"

    -- Note: SCATTER by applies to the TABLE value expression, it is not part of a normal SelectStmt.
    -- Pushing the SCATTER clause into the select statement is not supported syntax.
    -- The following is allowed syntax:
    SELECT * FROM transform( TABLE( (SELECT * FROM intable) SCATTER BY id) ) order by b;

