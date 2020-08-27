
-- GUC preserve_json_illegal_escape and bad escapes with \u tests for json and jsonb

-- Set GUC value
SET gp_json_preserve_ill_formed = true;
SET gp_json_preserve_ill_formed_prefix = "##";

-- first json

-- basic unicode input, the GUC doesn't handle those errors as well.
SELECT '"\u"'::json;			-- ERROR, incomplete escape
SELECT '"\u00"'::json;			-- ERROR, incomplete escape
SELECT '"\u000g"'::json;		-- ERROR, g is not a hex digit
SELECT '"\u0000"'::json;		-- OK, legal escape
SELECT '"\uaBcD"'::json;		-- OK, uppercase and lower case both OK

-- handling of unicode surrogate pairs

select json '{ "a":  "\ud83d\ude04\ud83d\udc36" }' -> 'a' as correct_in_utf8;
select json '{ "a":  "\ud83d\ud83d" }' -> 'a'; -- 2 high surrogates in a row
select json '{ "a":  "\ude04\ud83d" }' -> 'a'; -- surrogates in wrong order
select json '{ "a":  "\ud83dX" }' -> 'a'; -- orphan high surrogate
select json '{ "a":  "\ude04X" }' -> 'a'; -- orphan low surrogate

-- handling of simple unicode escapes

select json '{ "a":  "the Copyright \u00a9 sign" }' as correct_in_utf8;
select json '{ "a":  "dollar \u0024 character" }' as correct_everywhere;
select json '{ "a":  "dollar \\u0024 character" }' as not_an_escape;
select json '{ "a":  "null \u0000 escape" }' as not_unescaped;
select json '{ "a":  "null \\u0000 escape" }' as not_an_escape;

select json '{ "a":  "the Copyright \u00a9 sign" }' ->> 'a' as correct_in_utf8;
select json '{ "a":  "dollar \u0024 character" }' ->> 'a' as correct_everywhere;
select json '{ "a":  "dollar \\u0024 character" }' ->> 'a' as not_an_escape;
select json '{ "a":  "null \u0000 escape" }' ->> 'a' as fails;
select json '{ "a":  "null \\u0000 escape" }' ->> 'a' as not_an_escape;

-- ill-formed escaped sequences in key
select json '{ "\u0000": "abc" }' -> '##\u0000'; -- \u0000
select json '{ "\u0000X": "abc" }' -> '##\u0000X'; -- \u0000 followed by a ASCII char
select json '{ "Y\u0000": "abc" }' -> 'Y##\u0000'; -- \u0000 following a ASCII char
select json '{ "Y\u0000X": "abc" }' -> 'Y##\u0000X'; -- \u0000 in between two ASCII chars
select json '{ "\ud83dX": "abc" }' -> '##\ud83dX'; -- orphan high surrogate
select json '{ "\ude04X": "abc" }' -> '##\ude04X'; -- orphan low surrogate
select json '{ "\uDe04X": "abc" }' -> '##\uDe04X'; -- orphan low surrogate, the uppercase should be preserved


-- then jsonb

-- basic unicode input
SELECT '"\u"'::jsonb;			-- ERROR, incomplete escape
SELECT '"\u00"'::jsonb;			-- ERROR, incomplete escape
SELECT '"\u000g"'::jsonb;		-- ERROR, g is not a hex digit
SELECT '"\u0045"'::jsonb;		-- OK, legal escape
SELECT '"\u0000"'::jsonb;		-- ERROR, we don't support U+0000
-- use octet_length here so we don't get an odd unicode char in the
-- output
SELECT octet_length('"\uaBcD"'::jsonb::text); -- OK, uppercase and lower case both OK
-- since "\u0000" will be represented as the original escaped form string, it will has
-- the same length as "\\u0000"
SELECT octet_length('"\u0000"'::jsonb::text); -- OK, represented as "\u0000"
SELECT octet_length('"\u0000"'::jsonb::text); -- OK, totoally legal, represented as the same as the above one

-- handling of unicode surrogate pairs

SELECT octet_length((jsonb '{ "a":  "\ud83d\ude04\ud83d\udc36" }' -> 'a')::text) AS correct_in_utf8;
SELECT jsonb '{ "a":  "\ud83d\ud83d" }' -> 'a'; -- 2 high surrogates in a row
SELECT jsonb '{ "a":  "\ude04\ud83d" }' -> 'a'; -- surrogates in wrong order
SELECT jsonb '{ "a":  "\ud83dX" }' -> 'a'; -- orphan high surrogate
SELECT jsonb '{ "a":  "\ude04X" }' -> 'a'; -- orphan low surrogate

-- handling of simple unicode escapes

SELECT jsonb '{ "a":  "the Copyright \u00a9 sign" }' as correct_in_utf8;
SELECT jsonb '{ "a":  "dollar \u0024 character" }' as correct_everywhere;
SELECT jsonb '{ "a":  "dollar \\u0024 character" }' as not_an_escape;
SELECT jsonb '{ "a":  "null \u0000 escape" }' as fails;
SELECT jsonb '{ "a":  "null \\u0000 escape" }' as not_an_escape;

SELECT jsonb '{ "a":  "the Copyright \u00a9 sign" }' ->> 'a' as correct_in_utf8;
SELECT jsonb '{ "a":  "dollar \u0024 character" }' ->> 'a' as correct_everywhere;
SELECT jsonb '{ "a":  "dollar \\u0024 character" }' ->> 'a' as not_an_escape;
SELECT jsonb '{ "a":  "null \u0000 escape" }' ->> 'a' as fails;
SELECT jsonb '{ "a":  "null \\u0000 escape" }' ->> 'a' as not_an_escape;

-- ill-formed escaped sequences in key
SELECT jsonb '{ "\u0000": "abc" }' -> '##\u0000'; -- \u0000
SELECT jsonb '{ "\u0000X": "abc" }' -> '##\u0000X'; -- \u0000 followed by a ASCII char
SELECT jsonb '{ "Y\u0000": "abc" }' -> 'Y##\u0000'; -- \u0000 following a ASCII char
SELECT jsonb '{ "Y\u0000X": "abc" }' -> 'Y##\u0000X'; -- \u0000 in between two ASCII chars
SELECT jsonb '{ "\ud83dX": "abc" }' -> '##\ud83dX'; -- orphan high surrogate
SELECT jsonb '{ "\ude04X": "abc" }' -> '##\ude04X'; -- orphan low surrogate
SELECT jsonb '{ "\uDe04X": "abc" }' -> '##\uDe04X'; -- orphan low surrogate, the uppercase should be preserved

-- Inconsistencies
select json '{ "a":  "null \u0000 escape" }' -> 'a' as it_is;
select json '{ "a":  "null \u0000 escape" }' ->> 'a' as preserved_with_prefix;
SELECT jsonb '{ "a":  "null \u0000 escape" }' -> 'a' as preserved_with_prefix;
SELECT jsonb '{ "a":  "null \u0000 escape" }' ->> 'a' as preserved_with_prefix;
select '{ "a":  "dollar \u0000 character" }'::jsonb::json->'a' as preserved_with_prefix;
SELECT '{ "a":  "dollar \u0000 character" }'::json::jsonb->'a' as preserved_with_prefix;

-- empty prefix
SET gp_json_preserve_ill_formed_prefix = '';
SELECT jsonb '{ "a":  "null \u0000 escape" }' ->> 'a' as no_prefix;
SELECT jsonb '{ "a":  "\ude04\ud83d" }' -> 'a' as no_prefix; -- surrogates in wrong order
