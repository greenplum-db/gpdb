    -- ETF can be used for PREPARE statement
    PREPARE pretransform (int) AS SELECT * FROM transform( 
    TABLE(SELECT * FROM intable WHERE ID <=$1 ORDER BY ID SCATTER BY value)) 
    ORDER BY b;
    EXECUTE pretransform(5);
    EXECUTE pretransform(5);
    EXECUTE pretransform(5);
    EXECUTE pretransform(5);
    DEALLOCATE pretransform;
