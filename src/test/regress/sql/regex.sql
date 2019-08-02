--
-- Regular expression tests
--

-- Don't want to have to double backslashes in regexes
set standard_conforming_strings = on;

-- Test simple quantified backrefs
select 'bbbbb' ~ '^([bc])\1*$' as t;
select 'ccc' ~ '^([bc])\1*$' as t;
select 'xxx' ~ '^([bc])\1*$' as f;
select 'bbc' ~ '^([bc])\1*$' as f;
select 'b' ~ '^([bc])\1*$' as t;

-- Test quantified backref within a larger expression
select 'abc abc abc' ~ '^(\w+)( \1)+$' as t;
select 'abc abd abc' ~ '^(\w+)( \1)+$' as f;
select 'abc abc abd' ~ '^(\w+)( \1)+$' as f;
select 'abc abc abc' ~ '^(.+)( \1)+$' as t;
select 'abc abd abc' ~ '^(.+)( \1)+$' as f;
select 'abc abc abd' ~ '^(.+)( \1)+$' as f;

-- Test some cases that crashed in 9.2beta1 due to pmatch[] array overrun
select substring('asd TO foo' from ' TO (([a-z0-9._]+|"([^"]+|"")+")+)');
select substring('a' from '((a))+');
select substring('a' from '((a)+)');

-- Test conversion of regex patterns to indexable conditions
-- start_ignore
-- GPDB_93_MERGE_FIXME: the statistics and/or cost estimation code
-- in GPDB, combined with the much higher default random_page_cost
-- setting than PostgreSQL's, means that the planner chooses a seq scan
-- rather than an index scan for these. Force index scans, so that
-- the test tests what it's supposed to test. But we should investigate
-- why exactly the cost estimates are so different. There should be no
-- difference in the true cost of scans on catalog tables - they're not
-- even distributed.
set enable_seqscan=off;
set enable_bitmapscan=off;
-- end_ignore
explain (costs off) select * from pg_proc where proname ~ 'abc';
explain (costs off) select * from pg_proc where proname ~ '^abc';
explain (costs off) select * from pg_proc where proname ~ '^abc$';
explain (costs off) select * from pg_proc where proname ~ '^abcd*e';
explain (costs off) select * from pg_proc where proname ~ '^abc+d';
explain (costs off) select * from pg_proc where proname ~ '^(abc)(def)';
explain (costs off) select * from pg_proc where proname ~ '^(abc)$';
explain (costs off) select * from pg_proc where proname ~ '^(abc)?d';
explain (costs off) select * from pg_proc where proname ~ '^abcd(x|(?=\w\w)q)';

-- start_ignore
reset enable_seqscan;
reset enable_bitmapscan;
-- end_ignore

-- Test for infinite loop in pullback() (CVE-2007-4772)
select 'a' ~ '($|^)*';

-- These cases expose a bug in the original fix for CVE-2007-4772
select 'a' ~ '(^)+^';
select 'a' ~ '$($$)+';

-- More cases of infinite loop in pullback(), not fixed by CVE-2007-4772 fix
select 'a' ~ '($^)+';
select 'a' ~ '(^$)*';
select 'aa bb cc' ~ '(^(?!aa))+';
select 'aa x' ~ '(^(?!aa)(?!bb)(?!cc))+';
select 'bb x' ~ '(^(?!aa)(?!bb)(?!cc))+';
select 'cc x' ~ '(^(?!aa)(?!bb)(?!cc))+';
select 'dd x' ~ '(^(?!aa)(?!bb)(?!cc))+';

-- Test for infinite loop in fixempties() (Tcl bugs 3604074, 3606683)
select 'a' ~ '((((((a)*)*)*)*)*)*';
select 'a' ~ '((((((a+|)+|)+|)+|)+|)+|)';

-- These cases used to give too-many-states failures
select 'x' ~ 'abcd(\m)+xyz';
select 'a' ~ '^abcd*(((((^(a c(e?d)a+|)+|)+|)+|)+|a)+|)';
select 'x' ~ 'a^(^)bcd*xy(((((($a+|)+|)+|)+$|)+|)+|)^$';
select 'x' ~ 'xyz(\Y\Y)+';
select 'x' ~ 'x|(?:\M)+';

-- This generates O(N) states but O(N^2) arcs, so it causes problems
-- if arc count is not constrained
select 'x' ~ repeat('x*y*z*', 1000);

-- Test backref in combination with non-greedy quantifier
-- https://core.tcl.tk/tcl/tktview/6585b21ca8fa6f3678d442b97241fdd43dba2ec0
select 'Programmer' ~ '(\w).*?\1' as t;
select regexp_matches('Programmer', '(\w)(.*?\1)', 'g');

-- Test for proper matching of non-greedy iteration (bug #11478)
select regexp_matches('foo/bar/baz',
                      '^([^/]+?)(?:/([^/]+?))(?:/([^/]+?))?$', '');

-- Test that greediness can be overridden by outer quantifier
select regexp_matches('llmmmfff', '^(l*)(.*)(f*)$');
select regexp_matches('llmmmfff', '^(l*){1,1}(.*)(f*)$');
select regexp_matches('llmmmfff', '^(l*){1,1}?(.*)(f*)$');
select regexp_matches('llmmmfff', '^(l*){1,1}?(.*){1,1}?(f*)$');
select regexp_matches('llmmmfff', '^(l*?)(.*)(f*)$');
select regexp_matches('llmmmfff', '^(l*?){1,1}(.*)(f*)$');
select regexp_matches('llmmmfff', '^(l*?){1,1}?(.*)(f*)$');
select regexp_matches('llmmmfff', '^(l*?){1,1}?(.*){1,1}?(f*)$');

-- Test for infinite loop in cfindloop with zero-length possible match
-- but no actual match (can only happen in the presence of backrefs)
select 'a' ~ '$()|^\1';
select 'a' ~ '.. ()|\1';
select 'a' ~ '()*\1';
select 'a' ~ '()+\1';

-- Error conditions
select 'xyz' ~ 'x(\w)(?=\1)';  -- no backrefs in LACONs
select 'xyz' ~ 'x(\w)(?=(\1))';
select 'a' ~ '\x7fffffff';  -- invalid chr code
