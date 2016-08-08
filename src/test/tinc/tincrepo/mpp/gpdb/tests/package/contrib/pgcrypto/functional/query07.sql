select crypt('new password', 'crypt-key');

select crypt('new password', gen_salt('des'));

select crypt('new password', gen_salt('xdes',3));

select crypt('new password', gen_salt('md5'));

select crypt('new password', gen_salt('bf', 8));
