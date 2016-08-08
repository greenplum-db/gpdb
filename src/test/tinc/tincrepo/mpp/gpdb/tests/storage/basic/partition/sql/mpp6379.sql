create table mpp6379(a int, b date, primary key (a,b)) distributed by (a) partition by range (b) (partition p1 end ('2009-01-02'::date));

\d mpp6379*

insert into mpp6379( a, b ) values( 1, '20090101' );
insert into mpp6379( a, b ) values( 1, '20090101' );

alter table mpp6379 add partition p2 end(date '2009-01-03');
\d mpp6379_1_prt_p2

insert into mpp6379( a, b ) values( 2, '20090102' );
insert into mpp6379( a, b ) values( 2, '20090102' );

drop table mpp6379;
