set xmloption='document';
set optimizer_disable_missing_stats_collection = on;
\! gpfdist -p 45555 -d bugbuster/data &> bugbuster/data/xpath_gpfdist.out &
-- apparently this is needed on darwin
\! sleep 5

\echo --start_ignore
drop external table if exists readxml;
\echo --end_ignore

create readable external table readxml (text xml) location ('gpfdist://localhost:45555/CD.xml') format 'TEXT' (escape 'off' newline as 'CRLF');


select xml_in('<a></a>');
select xml_in('<a></>');
select xml_out((select xml_in('<a></a>')));
select xml_out((select xml_in('<a>hello</a>')));
select xml_out((select * from readxml));

--negative test

select xml_out((select xml_in('<a>hello<a>')));

select xmlcomment(E'<a>hello<\a>');
select xmlcomment('adsfasdfasdfsadf');
select xmlcomment(E'\n');
select xml('<hello></hello>');
select xml('<a><say>speak</say><hello>say hello</hello></a>');
select xml('hello');
select xml_send('<hello>say hello</hello>');
SELECT xmlconcat2('<abc/>', '<bar>foo</bar>');
SELECT xmlconcat2('<?xml version="1.1"?><foo/>', '<?xml version="1.1" standalone="no"?><bar/>');
---negative test ---
SELECT xmlconcat2('1', '2');
SELECT xmlconcat2(1, 2);
SELECT xmlconcat2('bad', '<syntax');
\echo --start_ignore
drop table if exists test_xpath;
\echo --end_ignore

CREATE TABLE test_xpath (y int, x xml);
INSERT INTO test_xpath VALUES (1, '<foo>abc</foo>');
INSERT INTO test_xpath VALUES (2, '<bar/>');
SELECT xmlagg(x order by y) FROM test_xpath;
SELECT xmlagg(x ORDER BY y DESC) FROM test_xpath;
\echo --start_ignore
drop table if exists xmlToText;
\echo --end_ignore
create table xmlToText(xmltext text);
insert into xmlToText((select text((select * from readxml))));

select * from xmlToText;

SELECT xpath('/my:a/value[.>15]',
'<my:a xmlns:my="http://example.com">
<value>20</value>
<value>10</value>
<value>30</value>
</my:a>',
ARRAY[ARRAY['my', 'http://example.com']]);

SELECT xpath('//mydefns:b/text()', '<a xmlns="http://example.com"><b>test</b></a>',
ARRAY[ARRAY['mydefns', 'http://example.com']]);

select xpath('//YEAR/parent::node()/YEAR[1]/text()',(select * from readxml));
select xpath('//CD[2]/YEAR/text()',(select * from readxml));

select xmlexists('//YEAR/parent::node()/YEAR[1]/text()',(select * from readxml));
select xmlexists('//CD[2]/YEAR/text()',(select * from readxml));

---- negative ----
select xmlexists('//CD[50]/YEAR/text()',(select * from readxml));

select xpath_exists('//YEAR/parent::node()/YEAR[1]/text()',(select * from readxml));
select xpath_exists('//CD[2]/YEAR/text()',(select * from readxml));
--- nagative ---
select xpath_exists('//CD[80]/YEAR',(select * from readxml));

SELECT xpath('/my:a/value[.>15]',
'<my:a xmlns:my="http://example.com">
<value>20</value>
<value>10</value>
<value>30</value>
</my:a>',
ARRAY[ARRAY['my', 'http://example.com']]);

SELECT xpath_exists('//mydefns:b/text()', '<a xmlns="http://example.com"><b>test</b></a>',
ARRAY[ARRAY['mydefns', 'http://example.com']]);

--- negative ---

SELECT xpath_exists('//mydefns:c/text()', '<a xmlns="http://example.com"><b>test</b></a>',
ARRAY[ARRAY['mydefns', 'http://example.com']]);

select xml_is_well_formed(text((select * from readxml)));

select xml_is_well_formed_document(text((select * from readxml)));
select xml_is_well_formed_content(text((select * from readxml)));

select xml_is_well_formed_content('<adf>adsfasdf<adf>');
select xml_is_well_formed_content('<adf>adsfasdf');

\! ps -ef | grep "gpfdist -p 45555" | grep -v grep | awk '{print $2}' | xargs kill &> bugbuster/data/xpath_kill_gpfdist.out

