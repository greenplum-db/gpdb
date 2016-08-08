-- Partition table

CREATE TABLE mdt_supplier_hybrid_part(
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
        values('6','11','1','7','16','2') WITH (checksum=false,appendonly=true,blocksize=1171456,compresslevel=3),
        values('18','20'),
        values('9','23','13') WITH (checksum=true,appendonly=true,blocksize=1335296,compresslevel=7),
        values('0','3','12','15','14','8','4','24','19','10','5')
)
(
partition p1 start('1') end('10001') every(10000)

);

drop table mdt_supplier_hybrid_part;

