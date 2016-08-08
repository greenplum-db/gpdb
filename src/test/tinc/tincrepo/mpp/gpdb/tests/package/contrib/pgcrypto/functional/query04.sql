-- kumara64=# select gen_salt('des');
--  gen_salt 
-- ----------
--  3G
-- (1 row)

select crypt('À1234abcd', '3G');
select crypt('À9234abcd', '3G');

-- kumara64=# select gen_salt('xdes', 725);
--  gen_salt  
-- -----------
--  _J9..LQde
-- (1 row)

select crypt('À1234abcd', '_J9..LQde');
select crypt('À9234abcd', '_J9..LQde');
