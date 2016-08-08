-- @description : ao and co compression tables
-- 

-- AO compr tables
DROP TABLE if exists sto_ao_compr1;
CREATE TABLE sto_ao_compr1(
          col_text text,
          col_numeric numeric
          CONSTRAINT tbl_chk_con1 CHECK (col_numeric < 250)
          ) with(appendonly=true, compresstype='zlib', compresslevel=1, blocksize=8192) DISTRIBUTED by(col_text);

insert into sto_ao_compr1 values ('0_zero',0);
insert into sto_ao_compr1 values ('1_one',1);
insert into sto_ao_compr1 values ('2_two',2);

select * from sto_ao_compr1;

DROP TABLE if exists sto_ao_compr2;
CREATE TABLE sto_ao_compr2(
          stud_id int,
          stud_name varchar(60) default 'default'
          ) with(appendonly=true, compresstype=quicklz, compresslevel=1) DISTRIBUTED by(stud_id);

insert into sto_ao_compr2 values ( 1,'annnnnnnnnnnnnnnnnnnnnnnn');
insert into sto_ao_compr2 values ( 2,'bennnnnnnnnnnnnnnnnnnnnnn');

select * from sto_ao_compr2;

-- CO compr tables
DROP TABLE if exists sto_co_compr1;
CREATE TABLE sto_co_compr1(
          col_text text,
          col_numeric numeric
          CONSTRAINT tbl_chk_con1 CHECK (col_numeric < 250)
          ) with(appendonly=true, orientation=column, compresstype='quicklz', compresslevel=1, blocksize=8192) DISTRIBUTED by(col_text);

insert into sto_co_compr1 values ('0_zero',0);
insert into sto_co_compr1 values ('1_one',1);
insert into sto_co_compr1 values ('2_two',2);

select * from sto_co_compr1;

DROP TABLE if exists sto_co_compr2;
CREATE TABLE sto_co_compr2(
            col_int int  ENCODING (compresstype=zlib,compresslevel=8),
            col_text text,
            DEFAULT COLUMN ENCODING (compresstype=rle_type,blocksize=8192,compresslevel=1)) 
            WITH (appendonly=true, orientation=column) distributed randomly ;

insert into sto_co_compr2 values(1, 'first row');
insert into sto_co_compr2 values(1, 'second row');

select * from sto_co_compr2;





