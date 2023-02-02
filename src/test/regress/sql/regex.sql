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

-- Test regexp_match()
select regexp_match('abc', '');
select regexp_match('abc', 'bc');
select regexp_match('abc', 'd') is null;
select regexp_match('abc', '(B)(c)', 'i');
select regexp_match('abc', 'Bd', 'ig'); -- error

-- Test lookahead constraints
select regexp_matches('ab', 'a(?=b)b*');
select regexp_matches('a', 'a(?=b)b*');
select regexp_matches('abc', 'a(?=b)b*(?=c)c*');
select regexp_matches('ab', 'a(?=b)b*(?=c)c*');
select regexp_matches('ab', 'a(?!b)b*');
select regexp_matches('a', 'a(?!b)b*');
select regexp_matches('b', '(?=b)b');
select regexp_matches('a', '(?=b)b');

-- Test lookbehind constraints
select regexp_matches('abb', '(?<=a)b*');
select regexp_matches('a', 'a(?<=a)b*');
select regexp_matches('abc', 'a(?<=a)b*(?<=b)c*');
select regexp_matches('ab', 'a(?<=a)b*(?<=b)c*');
select regexp_matches('ab', 'a*(?<!a)b*');
select regexp_matches('ab', 'a*(?<!a)b+');
select regexp_matches('b', 'a*(?<!a)b+');
select regexp_matches('a', 'a(?<!a)b*');
select regexp_matches('b', '(?<=b)b');
select regexp_matches('foobar', '(?<=f)b+');
select regexp_matches('foobar', '(?<=foo)b+');
select regexp_matches('foobar', '(?<=oo)b+');

-- Test optimization of single-chr-or-bracket-expression lookaround constraints
select 'xz' ~ 'x(?=[xy])';
select 'xy' ~ 'x(?=[xy])';
select 'xz' ~ 'x(?![xy])';
select 'xy' ~ 'x(?![xy])';
select 'x'  ~ 'x(?![xy])';
select 'xyy' ~ '(?<=[xy])yy+';
select 'zyy' ~ '(?<=[xy])yy+';
select 'xyy' ~ '(?<![xy])yy+';
select 'zyy' ~ '(?<![xy])yy+';

-- Test conversion of regex patterns to indexable conditions
-- start_ignore
-- GPDB_93_MERGE_FIXME: the statistics and/or cost estimation code
-- in GPDB, means that the planner chooses a seq scan
-- rather than an index scan for these. Force index scans, so that
-- the test tests what it's supposed to test. But we should investigate
-- why exactly the cost estimates are so different. There should be no
-- difference in the true cost of scans on catalog tables - they're not
-- even distributed.
-- 
-- After random_page_cost diminish, this guc still can not rid of.
-- Our index scan selectivity result have tiny different with upstream,
-- so that in a small table, e.g. pg_proc, planner will pick seq or bitmap
-- scan instead.
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

-- Test incorrect removal of capture groups within {0}
select 'xxx' ~ '(.){0}(\1)' as f;
select 'xxx' ~ '((.)){0}(\2)' as f;
select 'xyz' ~ '((.)){0}(\2){0}' as t;

-- Test ancient oversight in when to apply zaptreesubs
select 'abcdef' ~ '^(.)\1|\1.' as f;
select 'abadef' ~ '^((.)\2|..)\2' as f;

-- Error conditions
select 'xyz' ~ 'x(\w)(?=\1)';  -- no backrefs in LACONs
select 'xyz' ~ 'x(\w)(?=(\1))';
select 'a' ~ '\x7fffffff';  -- invalid chr code
