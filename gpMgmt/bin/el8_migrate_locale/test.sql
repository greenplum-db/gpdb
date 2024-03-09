-- case1 test basic table and index with char/varchar/text type
CREATE TABLE test_character_type
(
    char_1     CHAR(1),
    varchar_10 VARCHAR(10),
    txt        TEXT
);

INSERT INTO test_character_type (char_1)
VALUES ('Y    ') RETURNING *;

INSERT INTO test_character_type (varchar_10)
VALUES ('HelloWorld    ') RETURNING *;

INSERT INTO test_character_type (txt)
VALUES ('TEXT column can store a string of any length') RETURNING txt;

create index "test_id1 's " on test_character_type (char_1);
create index "test_id2 \ $ \\" on test_character_type (varchar_10);
create index " test_id "" 3 " on test_character_type (txt);

-- case2 test type citext;
create extension citext;
CREATE TABLE test_citext
(
    nick CITEXT PRIMARY KEY,
    pass TEXT NOT NULL
);

INSERT INTO test_citext VALUES ('larry', random()::text);
INSERT INTO test_citext VALUES ('Tom', random()::text);
INSERT INTO test_citext VALUES ('Damian', random()::text);
INSERT INTO test_citext VALUES ('NEAL', random()::text);
INSERT INTO test_citext VALUES ('Bjørn', random()::text);

create index test_idx_citext on test_citext (nick);

----- case 3 test special case with $
create table test1
(
    content varchar
) DISTRIBUTED by (content);
insert into test1 (content)
values ('a'),
       ('$a'),
       ('a$'),
       ('b'),
       ('$b'),
       ('b$'),
       ('A'),
       ('B');
create index id1 on test1 (content);

----  case4 test speical case with '""'
CREATE TABLE hash_test
(
    id   int,
    date text
) DISTRIBUTED BY (date);
insert into hash_test values (1, '01');
insert into hash_test values (1, '"01"');
insert into hash_test values (2, '"02"');
insert into hash_test values (3, '02');
insert into hash_test values (4, '03');

----  case5 test speical case with 1-1 vs 11
CREATE TABLE test2
(
    id   int,
    date text
) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( START (text '01-01') INCLUSIVE
   END (text '11-01') EXCLUSIVE
 );

insert into test2
values (2, '02-1'),
       (2, '03-1'),
       (2, '08-1'),
       (2, '09-01'),
       (1, '11'),
       (1, '1-1');

--- case6 test range partition with special character '“”'
CREATE TABLE partition_range_test
(
    id   int,
    date text
) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION Jan START ( '01') INCLUSIVE ,
      PARTITION Feb START ( '02') INCLUSIVE ,
      PARTITION Mar START ( '03') INCLUSIVE
      END ( '04') EXCLUSIVE);

insert into partition_range_test values (1, '01');
insert into partition_range_test values (1, '"01"');
insert into partition_range_test values (2, '"02"');
insert into partition_range_test values (2, '02');
insert into partition_range_test values (3, '03');
insert into partition_range_test values (3, '"03"');

-- case7 test range partition with default partition.
CREATE TABLE partition_range_test_default (id int, date text) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION feb START ( '02') INCLUSIVE ,
      PARTITION Mar START ( '03') INCLUSIVE,
      Default partition others);

insert into partition_range_test_default values (1, '01'), (1, '"01"'), (2, '"02"'), (2, '02'), (3, '03'), (3, '"03"'), (4, '04'), (4, '"04"');

-- case8 for testing insert into root select * from partition_range_test where date > '"02"';
create table root
(
    id   int,
    date text
) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
(PARTITION Jan START ( '01') INCLUSIVE ,
PARTITION Feb START ( '02') INCLUSIVE ,
PARTITION Mar START ( '03') INCLUSIVE
END ( '04') EXCLUSIVE);

insert into root
select *
from partition_range_test
where date > '"02"';

--- case9 test range partition with special character '“”' with ao
CREATE TABLE partition_range_test_ao
(
    id   int,
    date text
)
    WITH (appendonly = true)
    DISTRIBUTED BY (id)
    PARTITION BY RANGE (date)
    (PARTITION Jan START ('01') INCLUSIVE ,
    PARTITION Feb START ('02') INCLUSIVE ,
    PARTITION Mar START ('03') INCLUSIVE
    END ('04') EXCLUSIVE);

insert into partition_range_test_ao values (1, '01');
insert into partition_range_test_ao values (1, '"01"');
insert into partition_range_test_ao values (1, '"01-1"');
insert into partition_range_test_ao values (2, '"02-1"');
insert into partition_range_test_ao values (2, '"02"');
insert into partition_range_test_ao values (2, '02');

--- case10 for index constraint violation 
CREATE TABLE repository
(
    id         integer,
    slug       character varying(100),
    name       character varying(100),
    project_id character varying(100)
) DISTRIBUTED BY (slug, project_id);

insert into repository values (793, 'text-rnn', 'text-rnn', 146);
insert into repository values (812, 'ink_data', 'ink_data', 146);

-- case11 for index unique constraint violation 
create table gitrefresh
(
    projecttag        text,
    state             character(1),
    analysis_started  timestamp without time zone,
    analysis_ended    timestamp without time zone,
    counter_requested integer,
    customer_id       integer,
    id                int,
    constraint idx_projecttag unique (projecttag)
);
create index pk_gitrefresh on gitrefresh (id);
INSERT INTO gitrefresh(projecttag, state, analysis_started, counter_requested, customer_id)
VALUES ('npm@randombytes', 'Q', NOW(), 1, 0);

-- case12 for partition range list and special characters
CREATE TABLE rank
(
    id     int,
    gender char(1)
) DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
( PARTITION girls VALUES ('F'), 
  PARTITION boys VALUES ('M'), 
  DEFAULT PARTITION other );

CREATE TABLE "rank $ % &"
(
    id     int,
    gender char(1)
) DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
( PARTITION girls VALUES ('F'), 
  PARTITION boys VALUES ('M'), 
  DEFAULT PARTITION other );

CREATE TABLE "rank $ % & ! *"
(
    id     int,
    gender char(1)
) DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
( PARTITION girls VALUES ('F'), 
  PARTITION boys VALUES ('M'), 
  DEFAULT PARTITION other );

CREATE TABLE "rank 's "
(
    id     int,
    gender char(1)
) DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
( PARTITION girls VALUES ('F'), 
  PARTITION boys VALUES ('M'), 
  DEFAULT PARTITION other );

CREATE TABLE "rank 's' "
(
    id     int,
    gender char(1)
) DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
( PARTITION girls VALUES ('F'), 
  PARTITION boys VALUES ('M'), 
  DEFAULT PARTITION other );

CREATE TABLE "rank b c"
(
    id     int,
    gender char(1)
) DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
( PARTITION girls VALUES ('F'), 
  PARTITION boys VALUES ('M'), 
  DEFAULT PARTITION other );

-- case13 for testing partition key is type date
CREATE TABLE sales (id int, time date, amt decimal(10,2))
DISTRIBUTED BY (id)
PARTITION BY RANGE (time)
( START (date '2022-01-01') INCLUSIVE
   END (date '2023-01-01') EXCLUSIVE
   EVERY (INTERVAL '1 month') );

-- case14 for testing partition range with special characters in name
CREATE TABLE "partition_range_ 's " (id int, date text) 
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION feb START ( '02') INCLUSIVE ,
      PARTITION Mar START ( '03') INCLUSIVE,
      Default partition others);

CREATE TABLE "partition_range_ 's' " (id int, date text) 
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION feb START ( '02') INCLUSIVE ,
      PARTITION Mar START ( '03') INCLUSIVE,
      Default partition others);

-- case15 for check partition ddl broken
CREATE TABLE testddlbroken (id int, date text)
    DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
    (PARTITION Jan START ('01') END ('"01"'),
    PARTITION Feb START ('"01"') END ('02')
);
