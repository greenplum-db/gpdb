-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Create databases to be used for the test
-- start_ignore
Drop database dsp_db1;
Drop database dsp_db2;
Drop database dsp_db3;
Drop database dsp_db4;
-- end_ignore

Create database dsp_db1;
Create database dsp_db2;
Create database dsp_db3;
Create database dsp_db4;

-- Create roles to be used for the test

-- start_ignore
Drop role dsp_role1;
Drop role dsp_role2;
Drop role dsp_role3;
Drop role dsp_role4;
Drop role dsp_role5;
-- end_ignore

Create role dsp_role1 with login password 'dsprolepwd';
Create role dsp_role2 with login password 'dsprolepwd';
Create role dsp_role3 with login password 'dsprolepwd';
Create role dsp_role4 with login password 'dsprolepwd';
Create role dsp_role5 with login password 'dsprolepwd';
