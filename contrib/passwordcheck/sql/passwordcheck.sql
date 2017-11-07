LOAD 'passwordcheck';
CREATE USER regress_user1;

-- ok
ALTER USER regress_user1 PASSWORD 'a_nice_long_password';

-- error: too short
ALTER USER regress_user1 PASSWORD 'tooshrt';

-- error: contains user name
ALTER USER regress_user1 PASSWORD 'xyzregress_user1';

-- error: contains only letters
ALTER USER regress_user1 PASSWORD 'alessnicelongpassword';

DROP USER regress_user1;
