\df

select proname,pronamespace,proowner,prolang,proisagg,prosecdef,proisstrict,proretset,provolatile,pronargs,prorettype,proargtypes,prosrc from pg_proc where proname='scube';
select proname,pronamespace,proowner,prolang,proisagg,prosecdef,proisstrict,proretset,provolatile,pronargs,prorettype,proargtypes,prosrc from pg_proc where proname='old_scube';

