    -- Cannot drop describe (callback) function 
    -- while there is dyr table function (project) is using it
    DROP FUNCTION project_desc(internal);

