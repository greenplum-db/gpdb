
drop table if exists phone_book;

create table phone_book
(
lname char varying(25),
fname char varying(25),
state char(2),
phone_num bigint
)
distributed by (lname);

\copy public.phone_book from '%MYD%/phone_book.txt' delimiter as '|'

drop table if exists phone_book_substr;

create table phone_book_substr
(
lname_substr char(3),
lname char varying(25),
fname char varying(25),
state char(2),
phone_num bigint
)
distributed by (lname_substr);

insert into
phone_book_substr
(
lname_substr,
lname,
fname,
state,
phone_num
)
select
substr(lname,1,3),
lname,
fname,
state,
phone_num
from phone_book
order by lname
;
