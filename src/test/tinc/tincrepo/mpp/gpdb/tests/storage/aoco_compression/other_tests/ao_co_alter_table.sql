-- @product_version gpdb: [4.3.0.0-]
-- start_ignore
DROP TABLE IF EXISTS like1;
DROP TABLE IF EXISTS like2;
DROP TABLE IF EXISTS child2;
DROP TABLE IF EXISTS carpe_diem;
DROP TABLE IF EXISTS cot_table11;
-- end_ignore

CREATE TABLE cot_table11 (
 id BIGINT, 
 wealth MONEY CONSTRAINT wealth_not_null_constraint NOT NULL,
 crazy_like_a_box BOX, 
 pointless POINT, 
 outer_circle CIRCLE,
 uncharacteristic CHAR(1000) DEFAULT 'Twas brillig and the slithy toves...',
 thymestamp TIMESTAMP
 ) 
 WITH (
  APPENDONLY='True', 
  ORIENTATION='Column'
 )
 DISTRIBUTED BY (wealth);

INSERT INTO cot_table11 (
  id, 
  wealth,
  crazy_like_a_box, 
  pointless, 
  outer_circle,
  uncharacteristic,
  thymestamp
 )
 VALUES (
  2147483648,
  '6.75',
  '( (0, 0), (2, 2) )',
  '(-1, -1)',
  '( (0, -1), 3)',
  'I live in the mid-S.F. Bay '                      || 
     'now a suburb of northern L.A. '                ||
     'which extends from the south Baja coast '      ||
     'to the point of the snows northern-most.'      || 
     'Did you know that the high point in New York ' ||
     'is not a building or high-flying storks '      ||
     'but instead is the city landfill'              ||
     'which makes Everest seem a molehill?'          ||
     'And all the land we can see'                   ||
     'is piled high with quite-toxic debris.', 
  '2008-09-28 12:00:00'
 );


ALTER TABLE cot_table11 RENAME COLUMN thymestamp TO timeless;

-- This should fail because a column has been renamed but this uses the 
-- old name.
INSERT INTO cot_table11 (
  id, 
  wealth,
  crazy_like_a_box, 
  pointless, 
  outer_circle,
  uncharacteristic,
  thymestamp
  )
 VALUES (
  0,
  0,
  '( ( 19, 20), (20, 19) )',
  '(0,0)',
  '( (0,0) , 0)',
  'Present tense'  || 
  'the future is even more tense' ||
  'and I am long past tense',
  '2009-07-31 12:59:59'
 );


-- Rename the table twice.
ALTER TABLE cot_table11 RENAME TO high_seize;
ALTER TABLE high_seize RENAME TO carp_die;

ALTER TABLE carp_die RENAME uncharacteristic TO out_of_character;

ALTER TABLE carp_die SET DISTRIBUTED BY (timeless);
ALTER TABLE carp_die SET DISTRIBUTED randomly;


-- Make sure that we can still insert into the table after we have 
-- renamed the table and at least one of its columns and made other 
-- alterations.
INSERT INTO carp_die (
  id, 
  wealth,
  crazy_like_a_box, 
  pointless, 
  outer_circle,
  out_of_character,
  timeless
  )
 VALUES (
  -1,
  '7.25'::money,
  '((16, 16), (17, 17))', 
  '(-2147483648, -2147483648)',
  '((19, 21.5), 1)',
  'When you can walk on plastic '     ||
    'from Santa Catalina to Laguna '  || 
    'will you think it is fantastic'   ||
    'that there is more mercury than fish inside your Tuna?',
  '2020-02-29 23:59:59.999999'
 );

ALTER TABLE carp_die ALTER out_of_character SET DEFAULT 
 'Little tis the luck I have had, and oh tis comfort small ' || 
 'to think that many another lad has had no luck at all.'     ||
 ' -- e.e. cummings';

-- Insert a record and make sure that the default value for the 
-- out_of_character column was inserted.
INSERT INTO carp_die (id, wealth, timeless) VALUES (121, '121', '2010-03-02 15:45:00.00');
SELECT out_of_character FROM carp_die WHERE id = 121;

ALTER TABLE carp_die ALTER out_of_character DROP DEFAULT;

-- This time the out_of_character column for the new row should be empty 
-- because we dropped the DEFAULT value for the column.
INSERT INTO carp_die (id, wealth, timeless) VALUES (122, '122', '2010-03-02 15:46:00');
SELECT out_of_character FROM carp_die WHERE id = 122;


-- The table cannot have already been clustered, since we do not allow indexes 
-- on AO/COT tables, but let's just see what happens when we run this command.
ALTER TABLE carp_die SET WITHOUT CLUSTER;


DROP FUNCTION IF EXISTS money_to_text(MONEY) CASCADE;
CREATE FUNCTION money_to_text(MONEY) RETURNS TEXT AS
  $$
    SELECT textin(cash_out($1))
  $$ LANGUAGE SQL;
CREATE CAST (MONEY AS TEXT) 
 WITH FUNCTION money_to_text(money) 
 AS IMPLICIT
 ;

ALTER TABLE carp_die ALTER COLUMN wealth TYPE TEXT;

-- Try to violate the NOT NULL constraint on the wealth column.
INSERT INTO carp_die (id, wealth) VALUES (102, NULL);    -- explicit NULL
INSERT INTO carp_die (id) VALUES (102);                  -- implicit NULL

-- This command should affect performance, not correctness of output, and 
-- this test doesn't measure performance, so all we are really testing with 
-- the following statement is that we don't do something horribly bad.
ALTER TABLE carp_die ALTER COLUMN out_of_character SET STATISTICS 20;

-- Normally, this isn't the way you would add a NOT NULL constraint, but I 
-- believe that this is a reasonable thing to do, so let's try it.
ALTER TABLE carp_die ADD CONSTRAINT momomo CHECK (timeless IS NOT NULL);

-- These 2 inserts should fail since they violate the "momomo" constraint created above.
INSERT INTO carp_die (id, wealth, timeless) VALUES (1000001, 1000001, NULL);
INSERT INTO carp_die (id, wealth) VALUES (1000001, 1000001);

-- Drop the constraint.  
ALTER TABLE carp_die DROP CONSTRAINT momomo;

ALTER TABLE carp_die ADD COLUMN temporary1 VARCHAR DEFAULT 'Fleetwood MacVie' 
 CONSTRAINT cococo CHECK(temporary1 > 'A');
-- This should work.
INSERT INTO carp_die (id, wealth, temporary1) values (5, 10, 'Christine');
-- This should fail because '801Live' violates the constraint "temporary1 > 'A'".
INSERT INTO carp_die (id, wealth, temporary1) values (801, 11, '801Live');

-- There shouldn't be any rows that have num_wealth IS NULL.
SELECT COUNT(*) AS num_wealth_null 
 FROM carp_die 
 WHERE wealth IS NULL;

SELECT id, wealth, timeless, pointless, outer_circle, crazy_like_a_box
 FROM carp_die
 ORDER BY id
 ;

SELECT id, out_of_character
 FROM carp_die
 ORDER BY id
 ; 


-- Create a child table of carp_die, using the INHERITS clause, without 
-- specifying whether the table should be column-oriented.  
-- Should it inherit the column-orientedness of the parent?
CREATE TABLE carpe_diem () INHERITS (carp_die);

-- Normally, you wouldn't copy records from a parent table to a child table, 
-- but I think I'll try it.
INSERT INTO carpe_diem SELECT * FROM carp_die;
SELECT id, wealth
 FROM carpe_diem 
 ORDER BY id;

-- If the new (child) table is column-oriented, then it should follow the 
-- same restrictions as a column-oriented table.  Since deletes on 
-- column-oriented tables are illegal, this should be illegal, too.  
-- However, this isn't failing, which implies that the child table was not 
-- created as a column-oriented table.  Is that a bug, or is the user 
-- required to specify that a child table be column-oriented???!!!
DELETE FROM carpe_diem WHERE wealth = '$122.00';
SELECT id, wealth
 FROM carpe_diem 
 ORDER BY id;

-- Create a table with the same structure as carp_die, using the LIKE clause, 
-- without specifying whether the table should be column-oriented.  
-- Should it automatically acquire the column-orientedness of the parent?
CREATE TABLE like1 (LIKE carp_die);
INSERT INTO like1 SELECT * FROM carp_die;
SELECT id, wealth
 FROM like1 
 ORDER BY id;

-- If the new table is column-oriented, then it should follow the 
-- same restrictions as a column-oriented table.  Since deletes on 
-- column-oriented tables are illegal, this should be illegal, too.  
-- However, this isn't failing, which implies that the new table was not 
-- created as a column-oriented table.  Is that a bug, or is the user 
-- required to specify that the new table be column-oriented???!!!
DELETE FROM like1 WHERE wealth = '$121.00';
SELECT id, wealth
 FROM like1 
 ORDER BY id;


-- Create a table with the same structure as carp_die, using the LIKE clause, 
-- but this time specify that the table should be column-oriented.  
CREATE TABLE like2 (LIKE carp_die) WITH (appendonly=True, orientation='column');
INSERT INTO like2 SELECT * FROM carp_die;

-- If the new table is column-oriented, then it should follow the 
-- same restrictions as a column-oriented table.  Since deletes on 
-- column-oriented tables are illegal, this should be illegal, too.  
DELETE FROM like2 WHERE wealth = '$7.25';


ALTER TABLE carp_die DROP COLUMN temporary1;
SELECT temporary1 FROM carp_die;  -- Should fail, of course.

-- Should the column be dropped from the child table?  I think it should be, 
-- and this seems to show that indeed it is.
SELECT temporary1 from carpe_diem;

-- The column should NOT be dropped from the tables created via LIKE, because 
-- those tables "are completely decoupled after creation is complete", 
-- according to the GP Admin Guide.  Therefore, the following should succeed:
SELECT temporary1 from like1 ORDER BY temporary1;
SELECT temporary1 from like2 ORDER BY temporary1;


-- These should fail because the table no longer has this name.
DROP TABLE cot_table11;
DROP TABLE high_seize;

-- These should succeed.
DROP TABLE carp_die CASCADE;
DROP TABLE like1;
DROP TABLE like2;
DROP FUNCTION IF EXISTS money_to_text(MONEY) CASCADE;

