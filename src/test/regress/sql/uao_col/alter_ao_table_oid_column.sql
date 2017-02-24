-- WITH OIDS should not be allowed for column oriented tables.
create table sto_alt_uao2_oid (i int, j char(10)) with
(appendonly=true, orientation=column, oids=true) DISTRIBUTED RANDOMLY;
