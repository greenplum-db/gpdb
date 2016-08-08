create schema myschema;
create schema myschema_new;
create schema myschema_diff;

CREATE TABLE myschema.mdt_supplier_hybrid_part(
                S_SUPPKEY INTEGER,
                S_NAME CHAR(25),
                S_ADDRESS VARCHAR(40),
                S_NATIONKEY INTEGER,
                S_PHONE CHAR(15),
                S_ACCTBAL decimal,
                S_COMMENT VARCHAR(101)
                )
partition by range (s_suppkey)
subpartition by list (s_nationkey) subpartition template (
        values('22','21','17'),
        values('6','11','1','7','16','2') WITH (orientation='column', appendonly=true),
        values('18','20') WITH (checksum=false, appendonly=true,blocksize=1171456, compresslevel=3),
        values('9','23','13') WITH (checksum=true,appendonly=true,blocksize=1335296,compresslevel=7),
        values('0','3','12','15','14','8','4','24','19','10','5')
)
(
partition p1 start('1') end('10001') every(10000)

);

Vacuum myschema.mdt_supplier_hybrid_part;
ALTER TABLE myschema.mdt_supplier_hybrid_part SET SCHEMA myschema_new;
Vacuum myschema_new.mdt_supplier_hybrid_part;
ALTER TABLE myschema.mdt_supplier_hybrid_part_1_prt_p1 SET SCHEMA myschema_new;
ALTER TABLE myschema.mdt_supplier_hybrid_part_1_prt_p1_2_prt_1 SET SCHEMA myschema_diff;
Vacuum myschema_diff.mdt_supplier_hybrid_part_1_prt_p1_2_prt_1;


drop table myschema_new.mdt_supplier_hybrid_part;
drop schema myschema;
drop schema myschema_new;
drop schema myschema_diff;


