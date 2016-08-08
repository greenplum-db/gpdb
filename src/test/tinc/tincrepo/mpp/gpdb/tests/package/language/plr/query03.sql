CREATE OR REPLACE FUNCTION r_max (integer, integer) RETURNS integer AS '
    if (is.null(arg1) && is.null(arg2))
        return(NULL)
    if (is.null(arg1))
        return(arg2)
    if (is.null(arg2))
        return(arg1)
    if (arg1 > arg2)
       return(arg1)
    arg2
' LANGUAGE 'plr';

select r_max(1,10);
