-- @gucs gp_create_table_random_default_distribution=off
CREATE TABLE ao_compr01(
          col_text text,
          col_numeric numeric
          CONSTRAINT tbl_chk_con1 CHECK (col_numeric < 250)
          ) with(appendonly=true, compresstype='zlib', compresslevel=1, blocksize=8192) DISTRIBUTED by(col_text);

insert into ao_compr01 values ('0_zero',0);
insert into ao_compr01 values ('1_one',1);
insert into ao_compr01 values ('2_two',2);
