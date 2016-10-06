create table caps as SELECT now()::date as "MYDATE", 1::int as "ID";
create table cap_with_dquote as SELECT now()::date as "MY""DATE", 1::int as "ID";
create table cap_with_dquote2 as SELECT now()::date as "MY\DATE", 1::int as "ID";
create table cap_with_dquote3 as SELECT now()::date as "MY\""DATE", 1::int as "ID";
create table cap_with_dquote4 as SELECT now()::date as "'MY\DATE'", 1::int as "ID";
create table cap_with_squote as SELECT now()::date as "MY\'DATE", 1::int as "ID";
create table randomkey as SELECT now()::date as "MY\'DATE", 1::int as "ID" DISTRIBUTED RANDOMLY;
