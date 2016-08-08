-- start_ignore
drop table testusers;
-- end_ignore

CREATE TABLE testusers(username varchar(100) PRIMARY KEY, cryptpwd text, md5pwd text);
INSERT INTO testusers(username, cryptpwd, md5pwd) 
    VALUES ('robby', crypt('test', gen_salt('md5')), md5('test')),
        ('artoo', crypt('test',gen_salt('md5')), md5('test'));
        
SELECT username, cryptpwd, md5pwd
    FROM testusers;

-- successful login
 SELECT username 
    FROM testusers 
    WHERE username = 'robby' AND cryptpwd = crypt('test', cryptpwd);
-- successful login     
 SELECT username 
    FROM testusers 
    WHERE username = 'artoo' AND cryptpwd = crypt('test', cryptpwd);
    
-- unsuccessful login
 SELECT username 
    FROM testusers 
    WHERE username = 'artoo' AND cryptpwd = crypt('artoo', cryptpwd);
    
-- using md5
SELECT username
    FROM testusers
    WHERE username = 'robby' and md5pwd = md5('test');
